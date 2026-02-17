# GGUF Bundler Inference Example

This guide provides a quick example of how to bundle a model and perform inference using the self-contained executable.

## 1. Bundle the Model

First, ensure you have built the `bundler` and `llama-server`. Then, bundle a GGUF model into a single executable:

```bash
# Usage: ./bundler/bundler <SERVER_PATH> <MODEL_PATH> <OUTPUT_PATH>
./bundler/bundler vendor/llama.cpp/build/bin/llama-server models/qwen2.5-0.5b-instruct-q4_k_m.gguf qwen-bundle
```

## 2. Run the Bundled Executable

Start the server. You don't need to specify the `-m` or `--model` flag because the executable automatically detects the bundled model inside itself.

```bash
./qwen-bundle --host 127.0.0.1 --port 8080 --n-gpu-layers 0
```

*The server will log: `main: Bundled model detected at offset ...`*

## 3. Perform Inference (via CURL)

In a separate terminal, send a completion request to the running server:

```bash
curl -X POST http://127.0.0.1:8080/completion \
     -H "Content-Type: application/json" \
     -d '{
       "prompt": "The capital of France is",
       "n_predict": 10
     }'
```

### Expected Output

The server will return a JSON response similar to this:

```json
{
  "content": " Paris.\nFrance is a country in Europe.",
  "id_slot": 0,
  "stop": true,
  "model": "exe",
  "tokens_predicted": 10,
  "tokens_evaluated": 5,
  ...
}
```

Note that the `model` field in the response is reported as `"exe"`, indicating it was loaded from the bundled executable.
