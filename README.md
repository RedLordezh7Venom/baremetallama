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



if embedded GGUF exists → use it
else → normal file path
```
gguf-bundler/
 ├─ bundler/        ← CLI tool that creates exe
 ├─ runtime/        ← modified llama.cpp server
 └─ examples/
```