from __future__ import annotations

import os
from pathlib import Path
from typing import Any

import streamlit as st

from groq import Groq

try:
    from chatbot.repo_index import RepoIndex, load_repo_index
except ImportError:  # pragma: no cover - fallback for direct script execution
    from repo_index import RepoIndex, load_repo_index


APP_TITLE = "Zoho OS Repo Chat"
DEFAULT_MODEL = "llama-3.3-70b-versatile"


SYSTEM_PROMPT = """You are a repository-aware assistant for the Zoho OS project.

Rules:
- Answer using the repository context only when possible.
- When the user asks where code lives, give exact file paths and a short reason.
- If the user mentions files with @file syntax, treat those as primary context.
- Render shell commands, code, and diffs as fenced Markdown code blocks.
- If you are unsure, say what is missing and point to the file or docs that should be checked next.
- Keep answers structured and practical.
"""


def get_repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


@st.cache_resource(show_spinner=False)
def get_index() -> RepoIndex:
    return load_repo_index(get_repo_root())


def get_groq_client() -> Groq | None:
    key = (
        st.secrets.get("GROQ_API_KEY")
        if hasattr(st, "secrets") and "GROQ_API_KEY" in st.secrets
        else os.getenv("GROQ_API_KEY")
    )
    if not key:
        return None
    return Groq(api_key=key)


def init_state() -> None:
    defaults: dict[str, Any] = {
        "messages": [],
        "memory": "No long-term memory yet.",
        "mode": "General Q&A",
        "model": DEFAULT_MODEL,
        "pinned_files": [],
        "chat_id": 1,
    }
    for key, value in defaults.items():
        if key not in st.session_state:
            st.session_state[key] = value


def reset_chat() -> None:
    st.session_state.messages = []
    st.session_state.memory = "No long-term memory yet."
    st.session_state.chat_id += 1


def mode_to_prompt(mode: str) -> str:
    if mode == "Semantic analysis":
        return (
            "Focus on architecture, data flow, subsystem relationships, and design intent. "
            "Explain how parts connect, not just what they are."
        )
    if mode == "Code walkthrough":
        return (
            "Walk through the code carefully, line by line when useful, and explain control flow, "
            "key functions, and dependencies."
        )
    if mode == "Build / run help":
        return (
            "Prioritize build commands, run steps, dependencies, and failure modes. "
            "Give exact commands in code blocks."
        )
    return "Answer clearly and directly."


def search_mode_for(mode: str) -> str:
    if mode == "Semantic analysis":
        return "semantic"
    if mode == "Code walkthrough":
        return "code"
    if mode == "Build / run help":
        return "docs"
    return "general"


def update_memory(client: Groq, model: str, previous: str, user_text: str, assistant_text: str) -> str:
    prompt = f"""Update the memory summary for this ongoing repository chat.

Current memory:
{previous}

Latest turn:
User: {user_text}
Assistant: {assistant_text}

Write a compact memory summary with:
- user goal
- files or subsystems already discussed
- open questions or unresolved areas
"""
    response = client.chat.completions.create(
        model=model,
        messages=[
            {"role": "system", "content": "You maintain compact memory summaries for a code assistant."},
            {"role": "user", "content": prompt},
        ],
        temperature=0.2,
    )
    return response.choices[0].message.content.strip()


def build_messages(index: RepoIndex, user_text: str, mode: str, pinned_files: list[str], memory: str) -> list[dict[str, str]]:
    context = index.format_context(user_text, pinned_paths=pinned_files, mode=search_mode_for(mode), top_k=8)
    mode_prompt = mode_to_prompt(mode)
    return [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "system", "content": f"Chat mode: {mode}\n{mode_prompt}"},
        {"role": "system", "content": f"Conversation memory:\n{memory}"},
        {"role": "system", "content": f"Repository context:\n{context}"},
    ]


def render_sidebar(index: RepoIndex) -> None:
    st.sidebar.title("Controls")
    st.sidebar.selectbox(
        "Chat mode",
        ["General Q&A", "Semantic analysis", "Code walkthrough", "Build / run help"],
        key="mode",
    )
    st.sidebar.text_input("Model", key="model", help="Groq model name. Leave the default unless you need a different one.")
    st.sidebar.multiselect(
        "Pinned files",
        options=index.file_paths,
        key="pinned_files",
        help="Add files that must stay in context even if the current question is broad.",
    )
    st.sidebar.button("New chat", on_click=reset_chat, use_container_width=True)
    st.sidebar.caption("Tip: mention a file as `@file src/kernel/main.c` or `@src/kernel/main.c`.")

    with st.sidebar.expander("Project map", expanded=False):
        st.markdown(
            """
            - `src/boot/main.asm`: bootstrap, CPU checks, paging, long mode
            - `src/boot/long_mode_init.asm`: 64-bit handoff into `kmain()`
            - `src/kernel/main.c`: subsystem initialization order
            - `src/kernel/shell.c`: interactive shell and command handling
            - `docs-site/docs/overview.mdx`: project overview and reading order
            - `docs-site/docs/build-and-run.mdx`: build and run instructions
            """
        )


def render_references(index: RepoIndex, user_text: str, pinned_files: list[str]) -> list[str]:
    refs = list(index.resolve_mentions(user_text))
    for path in pinned_files:
        if path not in refs:
            refs.append(path)

    if not refs:
        return []

    st.subheader("Referenced files")
    for path in refs[:8]:
        st.markdown(f"**{path}**")
        st.caption(index.explain_file(path))
    return refs


def main() -> None:
    st.set_page_config(page_title=APP_TITLE, layout="wide")
    init_state()
    index = get_index()
    client = get_groq_client()

    st.title(APP_TITLE)
    st.caption("Ask how the OS is built, where subsystems live, or point to files with `@file path/to/file`.")

    render_sidebar(index)

    left, right = st.columns([2, 1], gap="large")

    with left:
        if st.session_state.messages:
            for message in st.session_state.messages:
                with st.chat_message(message["role"]):
                    st.markdown(message["content"])

        user_text = st.chat_input("Ask about the repo, a subsystem, or a file...")
        if user_text:
            st.session_state.messages.append({"role": "user", "content": user_text})

            refs = render_references(index, user_text, st.session_state.pinned_files)
            messages = build_messages(index, user_text, st.session_state.mode, refs, st.session_state.memory)
            messages.extend(st.session_state.messages[-12:])

            with st.chat_message("assistant"):
                if client is None:
                    answer = (
                        "Groq API key is not configured. Set `GROQ_API_KEY` in your environment or "
                        "in Streamlit secrets, then reload the app."
                    )
                    st.error(answer)
                else:
                    with st.spinner("Thinking through the repo..."):
                        response = client.chat.completions.create(
                            model=st.session_state.model,
                            messages=messages,
                            temperature=0.2,
                        )
                        answer = response.choices[0].message.content.strip()
                        st.markdown(answer)

            st.session_state.messages.append({"role": "assistant", "content": answer})

            if client is not None:
                try:
                    st.session_state.memory = update_memory(
                        client,
                        st.session_state.model,
                        st.session_state.memory,
                        user_text,
                        answer,
                    )
                except Exception:
                    pass

    with right:
        st.subheader("Memory")
        st.write(st.session_state.memory)
        st.subheader("Chat state")
        st.write(f"Mode: {st.session_state.mode}")
        st.write(f"Messages: {len(st.session_state.messages)}")
        st.write(f"Pinned files: {len(st.session_state.pinned_files)}")
        with st.expander("How to ask", expanded=False):
            st.markdown(
                """
                - `@src/kernel/main.c` to force a file into context
                - "Explain the boot chain"
                - "Where is the shell command parsing?"
                - "Show the build steps as commands"
                """
            )
        with st.expander("Good file targets", expanded=False):
            st.markdown(
                """
                - `src/boot/main.asm`
                - `src/boot/long_mode_init.asm`
                - `src/kernel/main.c`
                - `src/kernel/shell.c`
                - `docs-site/docs/overview.mdx`
                - `docs-site/docs/build-and-run.mdx`
                """
            )


if __name__ == "__main__":
    main()
