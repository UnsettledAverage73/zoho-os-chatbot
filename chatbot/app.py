from __future__ import annotations

import json
from datetime import datetime
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
        "history": [],
    }
    for key, value in defaults.items():
        if key not in st.session_state:
            st.session_state[key] = value


def reset_chat() -> None:
    if st.session_state.messages:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M")
        st.session_state.history.append({
            "id": st.session_state.chat_id,
            "time": timestamp,
            "summary": st.session_state.memory,
            "messages": list(st.session_state.messages)
        })
    st.session_state.messages = []
    st.session_state.memory = "No long-term memory yet."
    st.session_state.chat_id += 1


def restore_chat(data: dict) -> None:
    st.session_state.messages = data.get("messages", [])
    st.session_state.memory = data.get("memory", "Restored session.")
    st.session_state.chat_id = data.get("chat_id", st.session_state.chat_id + 1)
    st.toast("Chat restored!")


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

Write a compact memory summary (max 3 sentences) with:
- user goal
- files already discussed
- unresolved areas
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
    st.sidebar.text_input("Model", key="model")
    st.sidebar.multiselect("Pinned files", options=index.file_paths, key="pinned_files")
    
    col1, col2 = st.sidebar.columns(2)
    with col1:
        st.button("New chat", on_click=reset_chat, use_container_width=True)
    with col2:
        chat_data = {"messages": st.session_state.messages, "memory": st.session_state.memory, "chat_id": st.session_state.chat_id}
        st.download_button("Export", data=json.dumps(chat_data, indent=2), file_name=f"chat_{datetime.now().strftime('%Y%m%d')}.json", mime="application/json", use_container_width=True)

    uploaded_file = st.sidebar.file_uploader("Restore", type=["json"])
    if uploaded_file:
        try:
            data = json.load(uploaded_file)
            if st.sidebar.button("Confirm Load"): restore_chat(data)
        except Exception: st.sidebar.error("Invalid file")

    st.sidebar.divider()
    st.sidebar.subheader("Past Chats")
    for item in reversed(st.session_state.history):
        with st.sidebar.expander(f"Chat {item['id']} ({item['time']})"):
            st.write(item["summary"])
            if st.button("Load", key=f"load_{item['id']}"): restore_chat(item)


def render_references(index: RepoIndex, user_text: str, pinned_files: list[str]) -> list[str]:
    refs = list(index.resolve_mentions(user_text))
    for path in pinned_files:
        if path not in refs: refs.append(path)
    if not refs: return []
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
    render_sidebar(index)

    left, right = st.columns([2, 1], gap="large")
    with left:
        for m in st.session_state.messages:
            with st.chat_message(m["role"]): st.markdown(m["content"])
        user_text = st.chat_input("Ask about the repo...")
        if user_text:
            st.session_state.messages.append({"role": "user", "content": user_text})
            st.rerun()

    if st.session_state.messages and st.session_state.messages[-1]["role"] == "user":
        user_text = st.session_state.messages[-1]["content"]
        refs = render_references(index, user_text, st.session_state.pinned_files)
        msgs = build_messages(index, user_text, st.session_state.mode, refs, st.session_state.memory)
        msgs.extend(st.session_state.messages[-10:])
        with left:
            with st.chat_message("assistant"):
                if not client: st.error("No API key")
                else:
                    with st.spinner("Thinking..."):
                        resp = client.chat.completions.create(model=st.session_state.model, messages=msgs, temperature=0.2)
                        ans = resp.choices[0].message.content.strip()
                        st.markdown(ans)
                        st.session_state.messages.append({"role": "assistant", "content": ans})
                        st.session_state.memory = update_memory(client, st.session_state.model, st.session_state.memory, user_text, ans)
                        st.rerun()

    with right:
        st.subheader("Memory")
        st.info(st.session_state.memory)
        st.subheader("Stats")
        st.write(f"Messages: {len(st.session_state.messages)}")


if __name__ == "__main__":
    main()
