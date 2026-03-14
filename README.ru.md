# FunctionHandler

[English](README.md) | **Русский**

Сериализуемая data-driven система вызова функций для Unreal Engine 5. Настройте вызов и параметры в редакторе, выполните в рантайме через `ProcessEvent`.

![Hero Banner](screenshots/hero_banner.png)

## Обзор

**FunctionHandler** упаковывает ссылку на `UFunction` вместе со значениями параметров в одну сериализуемую структуру (`FFunctionHandler`). Вместо хардкода вызовов или цепочек Blueprint-нод вы описываете *что вызвать* и *с какими параметрами* как данные и выполняете когда угодно.

**Возможности:**
- Сериализуемая структура `FFunctionHandler`. Работает с SaveGame, репликацией, data assets
- Нативные редакторы свойств в Details panel (GameplayTag, ассеты, цвета и т.д.)
- Кастомные K2-ноды: **Execute Handler**, **Make Handler**, **Set/Get Handler Parameters**
- Типизированные return value и out-параметры через кастомные thunk'и Blueprint VM
- Шаблонный C++ API: `UFunctionHandlerLibrary::SetParameter<T>()`
- Runtime-модуль не зависит от editor-модулей

## Установка

Скачайте собранный плагин для UE 5.6 из [Releases](https://github.com/PsinaDev/FunctionHandler/releases/) или клонируйте/скачайте исходники. Поместите плагин в `Plugins/` вашего проекта, перегенерируйте файлы проекта и включите плагин в `.uproject` или через Edit > Plugins.

**Модули:**

| Модуль | Тип | Назначение |
|--------|-----|------------|
| `FunctionHandler` | Runtime | Структура, библиотека, VM thunk'и |
| `FunctionHandlerEditor` | Editor | Кастомизация свойств |
| `FunctionHandlerUncooked` | UncookedOnly | K2-ноды, виджеты пинов |

## Быстрый старт

### Blueprint: переменная

1. Создайте переменную типа `FFunctionHandler`
2. В Details panel выберите **Target Class** и **Function**
3. Настройте параметры нативными виджетами
4. Вызовите через ноду **Execute Function by Handler** или **Execute Handler**

![Variable Workflow](screenshots/variable_workflow.png)

### Blueprint: Make Handler

1. Поставьте ноду **Make Function Handler**
2. В деталях ноды укажите Target Class, выберите функцию из списка
3. Подключите типизированные входы
4. Выход соедините с **Execute Handler** для получения typed return values

![Make Handler Workflow](screenshots/make_handler_workflow.png)

### Blueprint: Set / Get Handler Parameters

**Set Handler Parameters** записывает все параметры существующего handler'а за раз, **Get Handler Parameters** читает их обратно. Оба с полностью типизированными пинами.

![Batch Set Get](screenshots/batch_set_get.png)

### Blueprint: установка одного параметра

1. Поставьте ноду **Set Handler Parameter**
2. Подключите переменную Handler или выход Make
3. Выберите параметр из списка. Пин Value автоматически подстроится под нужный тип

![Set Parameter Node](screenshots/set_parameter_node.png)

### C++

```cpp
#include "FunctionHandlerTypes.h"
#include "FunctionHandlerLibrary.h"

// Создаём handler
FFunctionHandler Handler;
Handler.TargetClass = UAbilityComponent::StaticClass();
Handler.FunctionName = TEXT("AddState");

// Задаём параметры (типобезопасно)
FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Active"));
UFunctionHandlerLibrary::SetParameter(Handler, TEXT("State"), Tag);

// Выполняем
UFunctionHandlerLibrary::ExecuteFunctionByHandler(MyAbilityComponent, Handler);
```

## Details Panel

Кастомизация свойств обеспечивает полноценное редактирование:

- Пикер **Target Class** со стандартным селектором классов
- Список **Function** с поиском (делегаты и служебные функции отфильтрованы)
- **Редакторы параметров** через нативные UE-виджеты под каждый тип
- Return value и чистые out-параметры скрыты автоматически

![Details Panel](screenshots/details_panel.png)

## K2-ноды

### Execute Function Handler

Вызывает handler на целевом объекте. Автоматически создаёт **типизированные выходные пины** под return value и out-параметры на основе сигнатуры функции.

**Особенности:**
- Резолвит сигнатуру из подключённой переменной (через CDO) или ноды Make
- Типизированные пины для return value и out-параметров
- Обновляется при компиляции Blueprint, изменении переменных и подключении пинов
- Оранжевый тинт для визуального отличия

### Make Function Handler

Конструирует `FFunctionHandler` инлайново с типизированными входными пинами под каждый параметр.

**Особенности:**
- Target Class в деталях ноды
- Список функций с поиском прямо на ноде
- Типизированные входные пины из сигнатуры функции
- Стыкуется с Execute Handler для сквозного typed workflow

### Set Handler Parameters

Пакетная запись всех параметров в существующий handler через типизированные входные пины. Сигнатура берётся из подключённого handler'а, по пину на каждый параметр.

**Особенности:**
- Типизированные входные пины под каждый параметр
- Работает и с переменными, и с выходом Make
- Пишутся только параметры с подключением или заданным default

### Get Handler Parameters

Пакетное чтение сохранённых параметров handler'а в типизированные выходные пины. Обратная операция к Set Handler Parameters.

**Особенности:**
- Типизированные выходные пины под каждый параметр
- Вычисляются только реально подключённые выходы
- Удобно для инспекции или проброса состояния handler'а

### Set Handler Parameter

Задаёт один параметр существующего handler'а через типобезопасный пин.

**Особенности:**
- Список параметров на ноде (подтягивается из функции подключённого handler'а)
- Wildcard-пин Value подстраивается под тип выбранного параметра
- Работает и с переменными, и с выходом Make

## Архитектура

```
FFunctionHandler (USTRUCT)
├── TargetClass: TSubclassOf<UObject>
├── FunctionName: FName
├── ParameterValues: TMap<FName, FString>    // сериализация через ExportText/ImportText
├── ResolveFunction(UObject*) -> UFunction*
└── ResolveFunctionFromClass() -> UFunction*

UFunctionHandlerLibrary (UCLASS)
├── ExecuteFunctionByHandler()               // fire-and-forget вызов
├── SetParameter<T>()                        // шаблонный setter для C++
├── InternalExecuteWithResult()              // возвращает UFunctionCallResult*
├── GetResultByName()                        // CustomThunk, типизированный выход
├── InternalSetParameter()                   // CustomThunk, типизированный вход
├── InternalGetParameter()                   // CustomThunk, чтение из TMap
└── InternalMakeFunctionHandler()            // конструирование структуры

UFunctionCallResult (UCLASS, Transient)
├── ResultData: TSharedPtr<FStructOnScope>   // владеет буфером параметров
├── CachedFunction: TWeakObjectPtr<UFunction>
└── GetBuffer() -> uint8*
```

### Как это устроено

1. **В редакторе:** кастомизация свойств создаёт `FStructOnScope(UFunction*)`, импортирует сохранённые значения и показывает их через `IStructureDetailsView`. При изменении значения экспортируются обратно в `TMap<FName, FString>`.

2. **При компиляции:** K2-ноды разворачиваются в цепочки `InternalMakeFunctionHandler` > `InternalSetParameter` > `InternalExecuteWithResult` > `GetResultByName`. CustomThunk'и используют `FProperty::ExportTextItem_Direct` / `ImportText_Direct` для типобезопасной конвертации.

3. **В рантайме:** `ExecuteFunctionByHandler` аллоцирует фрейм параметров (`FMemory::Malloc` + `InitializeStruct`), импортирует значения из TMap через `ImportText_Direct`, вызывает `ProcessEvent`, освобождает память.

### CustomThunk

Плагин использует механизм `CustomThunk` Blueprint VM для типобезопасной работы с wildcard-параметрами:

- **`GetResultByName`** читает из буфера `UFunctionCallResult` через `FProperty::CopySingleValue`
- **`InternalSetParameter`** экспортирует типизированное значение через `FProperty::ExportTextItem_Direct` в TMap handler'а
- **`InternalGetParameter`** импортирует текст из TMap handler'а обратно в типизированный выход через `FProperty::ImportText_Direct`

Все работают по паттерну `StepCompiledIn<FProperty>` с явным сбросом `MostRecentProperty` для защиты от устаревшего состояния VM.

## Требования

- Unreal Engine 5.6+
- C++17

## Лицензия

MIT
