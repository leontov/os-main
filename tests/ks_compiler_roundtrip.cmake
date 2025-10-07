# Автоматический тест для ks_compiler: кодирование и обратное декодирование.
set(sample_script "начало:\n    показать \"привет\"\nконец.\n")
set(sample_path "${CMAKE_CURRENT_BINARY_DIR}/ks_roundtrip.ks")
set(digits_path "${CMAKE_CURRENT_BINARY_DIR}/ks_roundtrip.ksd")
set(decoded_path "${CMAKE_CURRENT_BINARY_DIR}/ks_roundtrip_decoded.ks")

file(WRITE "${sample_path}" "${sample_script}")

execute_process(
    COMMAND "${ks_compiler}" "${sample_path}" -o "${digits_path}"
    RESULT_VARIABLE encode_result
)
if(NOT encode_result EQUAL 0)
    message(FATAL_ERROR "ks_compiler не смог закодировать файл")
endif()

execute_process(
    COMMAND "${ks_compiler}" --decode "${digits_path}" -o "${decoded_path}"
    RESULT_VARIABLE decode_result
)
if(NOT decode_result EQUAL 0)
    message(FATAL_ERROR "ks_compiler не смог декодировать цифровой файл")
endif()

file(READ "${decoded_path}" decoded_contents)
if(NOT decoded_contents STREQUAL sample_script)
    message(FATAL_ERROR "Декодированный текст не совпадает с исходным")
endif()
