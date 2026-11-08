# Whisper models

Model files (`*.bin`) are **not** stored in git. Put ggml Whisper models here (development builds
scan this folder) or in `%LOCALAPPDATA%\Stilleto\models`. You can also download them from the app's
**Models** page, which verifies the checksum before installing.

Source: <https://huggingface.co/ggerganov/whisper.cpp/tree/main>
(checksums from `external/whisper.cpp/models/README.md`)

| File | Size | SHA-1 | Notes |
| --- | --- | --- | --- |
| `ggml-tiny.en.bin` | 75 MiB | `c78c86eb1a8faa21b369bcd33207cc90d64ae9df` | Fastest |
| `ggml-base.en.bin` | 142 MiB | `137c40403d78fd54d454da0f9bd998f78703390c` | **Recommended** |
| `ggml-small.en.bin` | 466 MiB | `db8a495a91d927739e50b3fc1cc4c6b8f6c2d022` | More accurate, slower |
| `ggml-tiny.bin` | 75 MiB | `bd577a113a864445d4c299885e0cb97d4ba92b5f` | Multilingual |
| `ggml-base.bin` | 142 MiB | `465707469ff3a37a2b9b8d8f89f2f99de7299dac` | Multilingual |
| `ggml-small.bin` | 466 MiB | `55356645c2b361a969dfd0ef2c5a50d530afd8d5` | Multilingual |
| `ggml-large-v3-turbo-q5_0.bin` | 547 MiB | `e050f7970618a659205450ad97eb95a18d69c9ee` | Best accuracy; fast CPU or GPU |

PowerShell download example:

```powershell
Invoke-WebRequest https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin -OutFile models\ggml-base.en.bin
(Get-FileHash models\ggml-base.en.bin -Algorithm SHA1).Hash   # compare with the table above
```

`.en` models transcribe English only. Choose a multilingual model and set *Settings → Transcription →
Language* for other languages.
