# FunctionHandler

[English](README.md) | **Русский**

Сериализуемая data-driven система вызова функций для Unreal Engine 5. Настраивайте вызовы функций в редакторе с полным редактированием параметров — выполняйте их в рантайме через `ProcessEvent`.

![Hero Banner](screenshots/hero_banner.png)

## Обзор

**FunctionHandler** оборачивает ссылку на `UFunction` вместе с сохранёнными значениями параметров в единую сериализуемую структуру (`FFunctionHandler`). Вместо хардкода вызовов или прокладывания десятков Blueprint-нод, вы определяете *что* вызывать и *с какими параметрами* как данные — и выполняете это когда и где угодно.

**Ключевые возможности:**
- Сериализуемая структура `FFunctionHandler` — работает с SaveGame, репликацией, data assets
- Полноценные редакторы свойств в Details panel (пикеры GameplayTag, селекторы ассетов, пикеры цвета и т.д.)
- Кастомные K2-ноды: **Execute Handler**, **Make Handler**, **Set/Get Handler Parameters**
- Типизированные return value / out-параметры через кастомные thunk'и Blueprint VM
- C++ шаблонный API: `UFunctionHandlerLibrary::SetParameter<T>()`
- Нулевые runtime-зависимости от editor-модулей

## Установка

1. Склонируйте или скачайте в директорию `Plugins/` вашего проекта
2. Перегенерируйте файлы проекта
3. Включите плагин в `.uproject` или через Edit → Plugins

**Модули:**

| Модуль | Тип | Назначение |
|--------|-----|------------|
| `FunctionHandler` | Runtime | Основная структура, библиотека, VM thunk'и |
| `FunctionHandlerEditor` | Editor | Кастомизация свойств |
| `FunctionHandlerUncooked` | UncookedOnly | K2-ноды, виджеты пинов графа |

## Быстрый старт

### Blueprint — Workflow с переменной

1. Добавьте переменную `FFunctionHandler` в ваш Blueprint
2. В Details panel выберите **Target Class** и **Function**
3. Настройте параметры через нативные редакторы свойств
4. Используйте ноду **Execute Function by Handler** или **Execute Handler** в рантайме

![Variable Workflow](screenshots/variable_workflow.png)

### Blueprint — Workflow с Make Handler

1. Поставьте ноду **Make Function Handler**
2. Выберите Target Class в деталях ноды, выберите функцию из выпадающего списка
3. Подключите типизированные входные параметры
4. Соедините выход с **Execute Handler** для типизированных return values

![Make Handler Workflow](screenshots/make_handler_workflow.png)

### Blueprint — Set / Get Handler Parameters

Используйте **Set Handler Parameters** для записи всех параметров существующего handler'а одной нодой, и **Get Handler Parameters** для обратного чтения — оба с полностью типизированными пинами.

![Batch Set Get](screenshots/batch_set_get.png)

### Blueprint — Установка одного параметра

1. Поставьте ноду **Set Handler Parameter**
2. Подключите переменную Handler или ноду Make
3. Выберите параметр из выпадающего списка — пин Value автоматически резолвится в нужный тип

![Set Parameter Node](screenshots/set_parameter_node.png)

### C++

```cpp
#include "FunctionHandlerTypes.h"
#include "FunctionHandlerLibrary.h"

// Создаём handler
FFunctionHandler Handler;
Handler.TargetClass = UAbilityComponent::StaticClass();
Handler.FunctionName = TEXT("AddState");

// Устанавливаем параметры (типобезопасно)
FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Active"));
UFunctionHandlerLibrary::SetParameter(Handler, TEXT("State"), Tag);

// Выполняем на целевом объекте
UFunctionHandlerLibrary::ExecuteFunctionByHandler(MyAbilityComponent, Handler);
```

## Details Panel

Кастомизация свойств даёт полноценный опыт редактирования:

- Пикер **Target Class** со стандартным селектором классов
- Выпадающий список **Function** с поиском (фильтрует делегаты, внутренние функции)
- **Редакторы параметров** — нативные UE-виджеты для каждого типа свойств
- Скрытые параметры (return value, чистые out) автоматически отфильтрованы

![Details Panel](screenshots/details_panel.png)

## K2-ноды

### Execute Function Handler

Выполняет handler на целевом объекте. Автоматически генерирует **типизированные выходные пины** для return values и out-параметров на основе сигнатуры функции подключённого handler'а.

**Возможности:**
- Резолвит сигнатуру функции из подключённой переменной (через CDO) или ноды Make
- Типизированные пины return value и out-параметров
- Реагирует на компиляцию Blueprint, изменение переменных и подключение пинов
- Оранжевый оттенок для визуального отличия

### Make Function Handler

Создаёт структуру `FFunctionHandler` инлайново с типизированными входными пинами для каждого параметра функции.

**Возможности:**
- Target Class в деталях ноды
- Выпадающий список функций с поиском прямо на ноде
- Типизированные входные пины, сгенерированные из сигнатуры функции
- Цепочка с Execute Handler для сквозного типизированного workflow

### Set Handler Parameters

Пакетная запись всех значений параметров в существующий handler через типизированные входные пины. Резолвит сигнатуру функции из подключённого handler'а — генерирует по одному входному пину на параметр.

**Возможности:**
- Типизированные входные пины для каждого параметра функции
- Работает с handler'ами из переменных и из ноды Make
- Записывает только параметры с подключением или непустым default-значением

### Get Handler Parameters

Пакетное чтение всех сохранённых значений параметров из handler'а в типизированные выходные пины. Обратная операция к Set Handler Parameters.

**Возможности:**
- Типизированные выходные пины для каждого параметра функции
- Вычисляет только те выходы, которые реально подключены
- Полезно для инспекции или проброса состояния handler'а

### Set Handler Parameter

Устанавливает один параметр существующего handler'а через типобезопасный пин значения.

**Возможности:**
- Выпадающий список параметров на ноде (заполняется из функции подключённого handler'а)
- Wildcard-пин Value резолвится в тип выбранного параметра
- Работает как с handler'ами из переменных, так и из ноды Make

## Архитектура

```
FFunctionHandler (USTRUCT)
├── TargetClass: TSubclassOf<UObject>
├── FunctionName: FName
├── ParameterValues: TMap<FName, FString>    ← сериализация через ExportText/ImportText
├── ResolveFunction(UObject*) → UFunction*
└── ResolveFunctionFromClass() → UFunction*

UFunctionHandlerLibrary (UCLASS)
├── ExecuteFunctionByHandler()               ← простой fire-and-forget
├── SetParameter<T>()                        ← C++ шаблонный setter
├── InternalExecuteWithResult()              ← возвращает UFunctionCallResult*
├── GetResultByName()                        ← CustomThunk, типизированный выход
├── InternalSetParameter()                   ← CustomThunk, типизированный вход
├── InternalGetParameter()                   ← CustomThunk, типизированный выход из TMap
└── InternalMakeFunctionHandler()            ← конструирование структуры

UFunctionCallResult (UCLASS, Transient)
├── ResultData: TSharedPtr<FStructOnScope>   ← владеет буфером параметров
├── CachedFunction: TWeakObjectPtr<UFunction>
└── GetBuffer() → uint8*
```

### Как это работает

1. **Время редактирования:** Кастомизация свойств создаёт `FStructOnScope(UFunction*)`, импортирует сохранённые значения, отображает через `IStructureDetailsView`. Изменения экспортируются обратно в `TMap<FName, FString>`.

2. **Время компиляции:** K2-ноды разворачиваются в цепочки вызовов `InternalMakeFunctionHandler` → `InternalSetParameter` → `InternalExecuteWithResult` → `GetResultByName`. CustomThunk'и используют `FProperty::ExportTextItem_Direct` / `ImportText_Direct` для типобезопасной конвертации.

3. **Время выполнения:** `ExecuteFunctionByHandler` аллоцирует фрейм параметров (`FMemory::Malloc` + `InitializeStruct`), импортирует значения из TMap через `ImportText_Direct`, вызывает `ProcessEvent`, очищает память.

### Реализация CustomThunk

Плагин использует механизм `CustomThunk` Blueprint VM движка для типобезопасных wildcard-параметров:

- **`GetResultByName`** — читает из буфера `UFunctionCallResult` через `FProperty::CopySingleValue`
- **`InternalSetParameter`** — экспортирует типизированное значение через `FProperty::ExportTextItem_Direct` в TMap handler'а
- **`InternalGetParameter`** — импортирует сохранённый текст из TMap handler'а обратно в типизированный выход через `FProperty::ImportText_Direct`

Все следуют паттерну движка `StepCompiledIn<FProperty>` с явным сбросом `MostRecentProperty` для предотвращения устаревшего состояния VM.

## Требования

- Unreal Engine 5.6+
- C++17

## Лицензия

MIT
