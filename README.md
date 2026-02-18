proposed plan  (usage)

```
gguf-bundler/
 ├─ bundler/        ← CLI tool that creates exe
 ├─ runtime/        ← modified llama.cpp server
 └─ examples/
```

Final usage:

```
gguf-bundler pack \
  --server llama-server \
  --model tiny.gguf \
  --out tiny_server
```

Run:

```
./tiny_server
→ localhost:8080
```
Model used in tests and examples: `qwen2.5-0.5b-instruct_q4_K_M.gguf`


if embedded GGUF exists → use it
else → normal file path
s