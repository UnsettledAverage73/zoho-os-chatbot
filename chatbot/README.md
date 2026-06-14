# Zoho OS Repo Chat

This is a Streamlit app that answers questions about the Zoho OS repository with file-aware context, `@file` mentions, chat memory, and a semantic-analysis mode.

## Features

- repo indexing across source and docs
- `@file src/kernel/main.c` and `@src/kernel/main.c` style file mentions
- pinned files in the sidebar
- chat memory summary across turns
- mode switch for general Q&A, semantic analysis, walkthroughs, and build help
- Markdown output for code blocks and shell commands
- `New chat` reset

## Local run

```bash
cd /home/unsettledaverage73/zoho_os
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
export GROQ_API_KEY='your_groq_key'
streamlit run streamlit_app.py
```

## Hosting

The included `Dockerfile` is enough for Streamlit Cloud, Render, or any container host that can expose port `8501`.

## Notes

- The app does not hardcode the API key.
- Put the key in `GROQ_API_KEY` or `.streamlit/secrets.toml`.
