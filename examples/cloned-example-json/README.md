# cloned-example-json - задание целиком на JSON-сценариях

Никаких C++ плагинов: задача описана **только** через `tests/cases/*.json`.
Engine использует `JsonScenarioLoader`, преобразует JSON в `TestData`, runner
читает по ключам. Решение - последовательный код, поэтому
`allowedFrameworks: []` (никакой параллельный фреймворк не нужен).

Файлы:

```
tests/
  config.json                  # allowedFrameworks: [], mode: correctness
  runner/main.cpp              # читает array, считает sum/min/max/ok, пишет в output
  include/sum.h                # интерфейс задачи (последовательный)
  cases/sum.json               # сценарий с тремя тест-кейсами разных типов
  CMakeLists.txt               # пустой - нет .cpp плагинов
solution/
  CMakeLists.txt               # обычный sequential build
  include/sum.h
  src/sum.cpp                  # реализация
```

В `cases/sum.json` показано:
- input с массивом long long и булевым флагом,
- output с **несколькими ключами** разных типов (sum: int, max: int, ok: bool),
- сравнение по ключам автоматически.
