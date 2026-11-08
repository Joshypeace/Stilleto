# Unit tests for the pure core logic.
add_executable(StilletoCoreTests CoreTests.cpp TestFramework.h)
target_link_libraries(StilletoCoreTests PRIVATE StilletoCore)
target_compile_options(StilletoCoreTests PRIVATE /utf-8)
add_test(NAME core COMMAND StilletoCoreTests)

# Transcription integration test: loads a real model and transcribes whisper.cpp's JFK sample.
# Skips (exit code 0 with a message) when the model is not present.
add_executable(StilletoWhisperTests WhisperTests.cpp TestFramework.h)
target_link_libraries(StilletoWhisperTests PRIVATE StilletoCore)
target_compile_options(StilletoWhisperTests PRIVATE /utf-8)
target_compile_definitions(StilletoWhisperTests PRIVATE
    STILLETO_TEST_MODEL="${CMAKE_SOURCE_DIR}/models/ggml-base.en.bin"
    STILLETO_TEST_WAV="${STILLETO_WHISPER_DIR}/samples/jfk.wav")
add_test(NAME whisper COMMAND StilletoWhisperTests)

# Win32 integration tests: global hotkey and text insertion into a real Notepad window.
# These drive the desktop (they open Notepad and inject keystrokes), so they are not part of
# the default ctest run; launch StilletoPlatformTests manually on an unlocked desktop.
add_executable(StilletoPlatformTests PlatformTests.cpp TestFramework.h)
target_link_libraries(StilletoPlatformTests PRIVATE StilletoPlatform)
target_compile_options(StilletoPlatformTests PRIVATE /utf-8)
