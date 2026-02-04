#!/bin/bash
set -e

TEST_MODE=${1:-correctness}
THREADS=${OMP_NUM_THREADS:-4}

export OMP_NUM_THREADS=$THREADS

# Проверяем наличие решения студента
if [ ! -f /app/student_solution/parallel.cpp ]; then
    echo '{"error":"student solution not found","file":"parallel.cpp"}'
    exit 2
fi

# Сборка
mkdir -p /app/build && cd /app/build
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1

if [ "$TEST_MODE" = "correctness" ]; then
    if ! make correctness_tests 2>&1 | grep -i error; then
        ./correctness_tests
    else
        echo '{"error":"compilation failed","details":"check student code"}'
        exit 3
    fi
elif [ "$TEST_MODE" = "performance" ]; then
    if ! make performance_tests 2>&1 | grep -i error; then
        ./performance_tests
    else
        echo '{"error":"compilation failed","details":"check student code"}'
        exit 3
    fi
else
    echo '{"error":"unknown mode","mode":"'"$TEST_MODE"'"}'
    exit 1
fi
