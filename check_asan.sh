#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

REPORT_FILE="ASAN_report.txt"
BUILD_LOG_FILE="ASAN_build.log"
BUILD_DIR="build"
ASAN_BUILD_DIR="build-asan/build"
TARGET="MandelbrotFractal"
APP_PATH="${ASAN_BUILD_DIR}/${TARGET}"
SUPPRESSIONS_FILE="${SCRIPT_DIR}/asan.supp"

> "$REPORT_FILE"
> "$BUILD_LOG_FILE"

log_section() {
    echo "----------------------------------------" >> "$REPORT_FILE"
    echo "$1" >> "$REPORT_FILE"
    echo "----------------------------------------" >> "$REPORT_FILE"
}

echo "=== ASan check via CMake target ==="
if ! cmake -S . -B "$BUILD_DIR" >> "$BUILD_LOG_FILE" 2>&1; then
    echo "[Ошибка] Не удалось выполнить CMake configure. См. $BUILD_LOG_FILE" | tee -a "$REPORT_FILE"
    exit 1
fi

if ! cmake --build "$BUILD_DIR" --target asan-build >> "$BUILD_LOG_FILE" 2>&1; then
    echo "[Ошибка] Не удалось собрать target asan-build. См. $BUILD_LOG_FILE" | tee -a "$REPORT_FILE"
    exit 1
fi

if [ ! -x "$APP_PATH" ]; then
    echo "[Ошибка] ASan бинарник не найден: $APP_PATH" | tee -a "$REPORT_FILE"
    exit 1
fi

# успешная сборка — логируем в отчет
echo "[Успех] Бинарник успешно собран: ${APP_PATH}" | tee -a "$REPORT_FILE"

if [ ! -x "$APP_PATH" ]; then
    echo "[Ошибка] ASan бинарник не найден или нет прав на исполнение: $APP_PATH" | tee -a "$REPORT_FILE"
    exit 1
fi

# проверяем наличие файла подавлений перед запуском
if [ ! -f "$SUPPRESSIONS_FILE" ]; then
    echo "[Ошибка] Файл подавлений не найден: $SUPPRESSIONS_FILE" | tee -a "$REPORT_FILE"
    exit 1
fi

log_section "[Инфо] Запуск приложения с ASan"
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:${ASAN_OPTIONS:-}"
export UBSAN_OPTIONS="print_stacktrace=1:${UBSAN_OPTIONS:-}"
export LSAN_OPTIONS="suppressions=${SUPPRESSIONS_FILE}:print_suppressions=0:${LSAN_OPTIONS:-}"
echo "[Инфо] Подавления включены: $SUPPRESSIONS_FILE" | tee -a "$REPORT_FILE"

# запускаем приложение в интерактивном режиме.
"$APP_PATH" >> "$REPORT_FILE" 2>&1 &
APP_PID=$!
echo "[Инфо] Для завершения приложения нажмите Enter в окне" | tee -a "$REPORT_FILE"

# ожидаем штатного завершения процесса
wait "$APP_PID"
APP_EXIT=$?

# лог результата завершения
echo "----------------------------------------" >> "$REPORT_FILE"
if [ "$APP_EXIT" -eq 0 ]; then
    echo "[Успех] Приложение завершилось успешно (код 0)." >> "$REPORT_FILE"
else
    echo "[Ошибка] Приложение завершилось с ошибкой. Код возврата: ${APP_EXIT}." >> "$REPORT_FILE"
    echo "[Инфо] Проверьте логи выше на предмет утечек памяти (ASan/LSan) или падений (crash)." >> "$REPORT_FILE"
fi
echo "----------------------------------------" >> "$REPORT_FILE"


echo "==================================================="
echo "ASan проверка завершена с кодом: ${APP_EXIT}"
echo "Отчет ASan: $REPORT_FILE"
echo "Лог сборки: $BUILD_LOG_FILE"
exit $APP_EXIT
