# Root tests directory — delegates to unit/ and integration/
# Unit tests: fast, no engine runtime, cross-platform
# Integration tests: slow, need engine + Windows DLLs

add_subdirectory(unit)
add_subdirectory(integration)

# Optional: run unit tests as part of default 'test' target
# Integration tests run manually or on CI schedule