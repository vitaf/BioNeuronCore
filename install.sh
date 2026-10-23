#!/usr/bin/env bash
# Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
# E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
# Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.
#
# install.sh — установка и сборка BioNeuronCore
# Автоматически устанавливает системные зависимости, Intel oneAPI (опционально),
# собирает проект и запускает тесты.

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'
BOLD='\033[1m'; NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo -e "${CYAN}${BOLD}================================================${NC}"
echo -e "${CYAN}${BOLD}  Установка BioNeuronCore${NC}"
echo -e "${CYAN}${BOLD}  Вычислительный движок нейронных сетей${NC}"
echo -e "${CYAN}${BOLD}================================================${NC}"
echo ""

# ── Определение дистрибутива ────────────────────────────────────────────────

DISTRO=""
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO="${ID}"
fi

if [ "${DISTRO}" != "ubuntu" ] && [ "${DISTRO}" != "debian" ]; then
    echo -e "${YELLOW}⚠  Обнаружен дистрибутив: ${DISTRO} (не Ubuntu/Debian)${NC}"
    echo -e "${YELLOW}   Скрипт оптимизирован под Ubuntu/Debian. Продолжаем, но apt-команды могут не работать.${NC}"
    echo ""
fi

# ── Проверка sudo ───────────────────────────────────────────────────────────

SUDO=""
if command -v sudo &>/dev/null; then
    SUDO="sudo"
fi

# ── Шаг 1: Системные зависимости ────────────────────────────────────────────

echo -e "${BOLD}[1/6] Установка системных зависимостей...${NC}"

PKGS="build-essential cmake g++ pkg-config wget gnupg"

MISSING=""
for pkg in ${PKGS}; do
    if ! dpkg -s "${pkg}" &>/dev/null 2>&1; then
        MISSING="${MISSING} ${pkg}"
    fi
done

if [ -n "${MISSING}" ]; then
    echo "  Устанавливаю:${MISSING}"
    ${SUDO} apt-get update -qq
    ${SUDO} apt-get install -y -qq ${MISSING}
else
    echo "  ✓ Все базовые пакеты установлены."
fi

# ── Шаг 2: Intel oneAPI (опционально) ────────────────────────────────────────

echo ""
echo -e "${BOLD}[2/6] Проверка Intel oneAPI...${NC}"

ONEAPI_INSTALLED=false
if [ -f /opt/intel/oneapi/setvars.sh ]; then
    ONEAPI_INSTALLED=true
    echo "  ✓ Intel oneAPI найден в /opt/intel/oneapi/."
elif command -v dpcpp &>/dev/null 2>&1; then
    ONEAPI_INSTALLED=true
    echo "  ✓ Компилятор dpcpp найден в PATH."
elif command -v icpx &>/dev/null 2>&1; then
    ONEAPI_INSTALLED=true
    echo "  ✓ Компилятор icpx найден в PATH."
else
    echo -e "  ${YELLOW}⚠  Intel oneAPI не найден.${NC}"
    echo "  Без него GPU-ускорение (SYCL) будет недоступно."
    echo "  Будет собрана только CPU-версия (GCC)."
    echo ""
    echo "  Установить Intel oneAPI Base Toolkit:"
    echo "    1. Скачайте с https://www.intel.com/content/www/us/en/developer/tools/oneapi/base-toolkit.html"
    echo "    2. Или выполните (требуется ~4 ГБ свободного места):"
    echo ""
    echo "       wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | \\"
    echo "         gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null"
    echo "       echo \"deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main\" | \\"
    echo "         sudo tee /etc/apt/sources.list.d/oneAPI.list"
    echo "       sudo apt update && sudo apt install intel-basekit"
    echo ""
    read -r -p "  Установить Intel oneAPI сейчас? (y/N) " yn
    if [ "${yn}" = "y" ] || [ "${yn}" = "Y" ]; then
        echo "  Добавляю репозиторий Intel и устанавливаю intel-basekit..."
        wget -q -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | \
            gpg --dearmor | ${SUDO} tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
        echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" | \
            ${SUDO} tee /etc/apt/sources.list.d/oneAPI.list > /dev/null
        ${SUDO} apt-get update -qq
        ${SUDO} apt-get install -y intel-basekit
        ONEAPI_INSTALLED=true
        echo "  ✓ Intel oneAPI установлен."
    fi
fi

# ── Шаг 3: Проверка CMake версии ─────────────────────────────────────────────

echo ""
echo -e "${BOLD}[3/6] Проверка версии CMake...${NC}"

CMAKE_VER=$(cmake --version 2>/dev/null | head -1 | grep -oP '\d+\.\d+' || echo "0.0")
CMAKE_MAJ=$(echo "${CMAKE_VER}" | cut -d. -f1)
CMAKE_MIN=$(echo "${CMAKE_VER}" | cut -d. -f2)

echo "  Установлена версия CMake: ${CMAKE_VER}"

if [ "${CMAKE_MAJ}" -lt 3 ] || ([ "${CMAKE_MAJ}" -eq 3 ] && [ "${CMAKE_MIN}" -lt 20 ]); then
    echo -e "  ${YELLOW}⚠  Требуется CMake 3.20+. Устанавливаю...${NC}"
    CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v3.28.0/cmake-3.28.0-linux-x86_64.sh"
    TMP_SH="/tmp/cmake_install.sh"
    wget -q "${CMAKE_URL}" -O "${TMP_SH}"
    chmod +x "${TMP_SH}"
    ${SUDO} "${TMP_SH}" --prefix=/usr/local --skip-license
    rm -f "${TMP_SH}"
    echo "  ✓ CMake обновлён."
fi

# ── Шаг 4: Активация SYCL-окружения ──────────────────────────────────────────

echo ""
echo -e "${BOLD}[4/6] Настройка окружения SYCL...${NC}"

if [ "${ONEAPI_INSTALLED}" = true ]; then
    if [ -f /opt/intel/oneapi/setvars.sh ]; then
        echo "  Активирую oneAPI окружение..."
        source /opt/intel/oneapi/setvars.sh --include-intel-llvm > /dev/null 2>&1 || true
    fi
    if command -v dpcpp &>/dev/null 2>&1; then
        echo "  ✓ Компилятор: $(dpcpp --version 2>&1 | head -1)"
    elif command -v icpx &>/dev/null 2>&1; then
        echo "  ✓ Компилятор: icpx (SYCL через -fsycl)"
    fi
else
    echo "  SYCL неактивен — будет собрана только CPU-версия."
fi

# ── Шаг 5: Сборка ────────────────────────────────────────────────────────────

echo ""
echo -e "${BOLD}[5/6] Сборка BioNeuronCore...${NC}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "  Запускаю cmake..."
cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -3

echo "  Сборка (make -j$(nproc))..."
make -j"$(nproc)" 2>&1 | tail -5

echo -e "  ${GREEN}✓ Сборка завершена.${NC}"

# ── Шаг 6: Тесты ─────────────────────────────────────────────────────────────

echo ""
echo -e "${BOLD}[6/6] Запуск тестов...${NC}"

if ctest --output-on-failure 2>&1; then
    echo ""
    echo -e "${GREEN}${BOLD}================================================${NC}"
    echo -e "${GREEN}${BOLD}  Установка BioNeuronCore завершена успешно!${NC}"
    echo -e "${GREEN}${BOLD}================================================${NC}"
else
    echo ""
    echo -e "${RED}${BOLD}⚠  Некоторые тесты не прошли. Проверьте вывод выше.${NC}"
    exit 1
fi

echo ""
echo "  Исполняемые файлы находятся в: ${BUILD_DIR}/"
echo "  Для повторной активации SYCL: source /opt/intel/oneapi/setvars.sh"
echo ""
