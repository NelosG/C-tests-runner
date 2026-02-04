# Используем официальный образ с g++, в нём есть CMake
FROM gcc:12

# Устанавливаем необходимые утилиты и OpenMP (libomp)
RUN apt-get update && apt-get install -y cmake libomp-dev

# Копируем весь проект внутрь контейнера
COPY . /project

# Рабочая директория для сборки
WORKDIR /project

# Сборка проекта: CMake + make (команда может отличаться в вашем CMakeLists)
RUN cmake . && make -j$(nproc)

# Команда по умолчанию – запуск тестового движка
CMD ["./test_engine"]


#FROM ubuntu:20.04
#
## Устанавливаем инструменты сборки: cmake, компилятор, OpenMP и т.д.
#RUN apt-get update && apt-get install -y \
#    cmake \
#    g++ \
#    make \
#    git \
#    libomp-dev
#
## Копируем проект в контейнер
#WORKDIR /workspace
#COPY . /workspace
#
## Сборка проекта
#RUN mkdir build && cd build && cmake .. && make
#
## для корректности
#CMD ["bash", "-lc", "cd build && ./test_runner"]
#
## для производительности
#CMD ["bash", "-lc", "cd build && ./perf_runner"]
#
