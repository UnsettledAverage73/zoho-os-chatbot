FROM python:3.11-slim

WORKDIR /app

COPY chatbot/requirements.txt /app/chatbot/requirements.txt
COPY requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r /app/requirements.txt

COPY . /app

EXPOSE 8501

CMD ["streamlit", "run", "chatbot/app.py", "--server.address=0.0.0.0", "--server.port=8501"]
