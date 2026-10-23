# Установка и сборка BioNeuronCore

## Системные требования

- **ОС:** Linux (рекомендуется Ubuntu 22.04/24.04)
- **Компилятор:** Intel oneAPI DPC++ (dpcpp/icpx) с поддержкой SYCL 2020; или native CUDA (nvcc) для NVIDIA GPU
- **CMake:** 3.20+
- **Стандарт:** C++17

## Установка зависимостей

### Шаг 1: Intel oneAPI Base Toolkit

Установите Intel oneAPI Base Toolkit (содержит компилятор DPC++ и runtime SYCL):

```bash
# Добавление репозитория Intel
wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | \
  gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" | \
  sudo tee /etc/apt/sources.list.d/oneAPI.list

# Установка
sudo apt update
sudo apt install intel-basekit
```

Путь установки: `/opt/intel/oneapi/`

### Шаг 2: Активация окружения

```bash
source /opt/intel/oneapi/setvars.sh
```

Для автоматической активации добавьте в `~/.bashrc`:
```bash
echo 'source /opt/intel/oneapi/setvars.sh > /dev/null 2>&1' >> ~/.bashrc
```

### Шаг 3: Проверка

```bash
which dpcpp    # /opt/intel/oneapi/compiler/latest/bin/dpcpp
dpcpp --version
```

Если `dpcpp` не найден, CMake попытается использовать `icpx -fsycl`.

### Альтернатива: Native CUDA (NVIDIA GPU)

Для NVIDIA GPU используйте нативный nvcc (установите nvidia-cuda-toolkit):

```bash
# Установка через пакетный менеджер вашего дистрибутива или сборка из исходников
# Необходимо наличие CUDA (NVIDIA) или ROCm (AMD)
```

CMake автоматически определит nvcc и соберёт нативный CUDA-бекенд.

## Зависимости (подтягиваются автоматически)

- **Catch2 v2.13.10** — фреймворк тестирования (FetchContent, не требует ручной установки)

Все остальные зависимости — заголовки SYCL из oneAPI.

## Сборка

```bash
cd BioNeuronCore
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Цели сборки

| Цель | Тип | Описание |
|------|-----|----------|
| `biocore` | Статическая библиотека | CPU-версия (только HH + рецепторы + .bion-загрузчик) |
| `biocore_sycl` | Статическая библиотека | SYCL-версия (включает ядра GPU, симуляцию, .bion) |
| `test_hh` | Исполняемый файл | Тесты Hodgkin-Huxley (CPU) |
| `test_synapses` | Исполняемый файл | Тесты синаптических рецепторов (CPU) |
| `test_bion` | Исполняемый файл | Тесты бинарного формата .bion (CPU) |
| `test_consolidation` | Исполняемый файл | Тесты консолидации: Nucleus, модификаторы 87-92, ген→белок→структура |
| `test_nb_receptors` | Исполняемый файл | Тесты NB_-рецепторов (24 теста, Neuroblox-совместимые) |
| `test_gpu` | Исполняемый файл | Интеграционный тест GPU-симуляции (только при SYCL) |

### Сборка конкретной цели

```bash
make test_hh            # сборка + линковка CPU-теста HH
make test_synapses      # сборка + линковка CPU-теста синапсов
make test_plugins       # сборка + линковка CPU-теста плагинов
make test_nb_receptors  # сборка + линковка NB_-рецепторов (24 теста)
make test_bion          # сборка + линковка CPU-теста .bion
make test_gpu           # сборка + линковка GPU-теста (требует SYCL)
make biocore_sycl       # сборка SYCL-библиотеки
```

## Запуск тестов

```bash
# Отдельные тесты
./test_hh
./test_synapses
./test_bion
./test_gpu           # Требует GPU или упадёт на CPU с fallback

# Все тесты через CTest
ctest                # запуск test_hh + test_synapses + test_bion [+ test_gpu при SYCL]
```

## Ожидаемый вывод

```
$ ./test_hh
===============================================================================
All tests passed (10 assertions in 7 test cases)

$ ./test_gpu
GPU-устройство: Intel(R) UHD Graphics 630
Очередь создана.
Запуск симуляции на 500 тиков...
Симуляция завершена.
После 500 тиков (SYCL): V_m = -67.11 мВ
Проверка: V_m в диапазоне [-75, -55]: OK
```

## Возможные проблемы

### «dpcpp: command not found»
oneAPI не активирован. Выполните `source /opt/intel/oneapi/setvars.sh`.

### «SYCL: не найден — только CPU-тесты»
Компилятор SYCL не обнаружен. Установите Intel oneAPI или NVIDIA CUDA Toolkit. Без SYCL собираются только CPU-тесты (`test_hh`, `test_synapses`, `test_bion`).

### «GPU не найден, пробуем CPU...» (в test_gpu)
Штатная ситуация при отсутствии GPU. Тест выполнится на CPU через `cpu_selector_v`.

### «error: SYCL kernel cannot call undefined function»
Функция не помечена `BIOCORE_DEVICE_CALLABLE`. Добавьте макрос к определению и extern-декларации функции, вызываемой из SYCL-ядра.

### Выбор GPU-бекенда

MindCopy поддерживает три варианта GPU-ускорения:

| Бекенд | GPU | Лицензия | Компилятор |
|--------|-----|----------|------------|
| **Intel oneAPI** (по умолчанию) | Intel UHD/Iris/Xe | Бесплатно | `icpx -fsycl` |
| **Native CUDA (nvcc)** | NVIDIA | Бесплатно (CUDA Toolkit) | `nvcc` |
| **Codeplay oneAPI for NVIDIA** | NVIDIA | Community (бесплатно для некоммерческого использования) | `icpx -fsycl` |

#### Intel oneAPI (по умолчанию)
```bash
source /opt/intel/oneapi/setvars.sh
cmake .. -DCMAKE_CXX_COMPILER=icpx -DCMAKE_CXX_FLAGS="-fsycl"
```

#### Native CUDA (NVIDIA)
```bash
# Установка: sudo apt install nvidia-cuda-toolkit
cmake .. -DCMAKE_BUILD_TYPE=Release
```
Код MindCopy не требует изменений — nvcc поддерживает C++17 с расширениями CUDA.

#### Codeplay oneAPI for NVIDIA (Community Edition)
```bash
# Скачать с developer.codeplay.com (бесплатно для некоммерческого использования)
# После установки плагина NVIDIA GPU становится видимым через sycl-ls
source /opt/intel/oneapi/setvars.sh
cmake .. -DCMAKE_CXX_COMPILER=icpx -DCMAKE_CXX_FLAGS="-fsycl"
```


## Права и интеллектуальная собственность

© 2024–2026 **Фешин Виталий Александрович** / Vitaly Alexandrovich Feshin. All rights reserved.
E-mail: feshin2004@mail.ru · vfeshin@mindcopy.ru · Тел.: +7(918)345-75-18. Краснодар, Россия / Krasnodar, Russia.

Свидетельство о государственной регистрации программы для ЭВМ (Роспатент) **№ 727894_683451**.
Коммерческое использование, распространение и сублицензирование без письменного разрешения автора запрещены.
Полный текст — см. `LICENSE` и `DISCLAIMER.md` в корне платформы.
