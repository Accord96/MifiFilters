Мой первый Mini Filter Driver для Windows
Функционал драйвер:
1. Загрузка драйвера в момент старта ОС или вручную
2. Драйвер собирается под архитектуру x32 и x64
3. Синхронное логирование операций в файл
4. Протоколируемые операции: открытие/создание файла, запись в файл, установка информации о файле (без установки security descriptor), закрытие файла

Данные которые протоколируются:

a. Создание/открытие файла:
1. Полный путь к файлу
2. PID процесса
3. TID потока
4. Файловый объект
5. Создан ли новый файл или открыт существующий.

b. Запись в файл:
1. Полный путь к файлу
2. PID процесса
3. TID потока
4. Файловый объект
5. Уровень IRQL
6. Размер записанного блока данных.

c. Установка информации о файле:
1. Полный путь к файлу
2. PID процесса
3. TID потока
4. Файловый объект
5. Уровень IRQL
6. Тип устанавливаемой информации

d. Закрытие файла:
1. Полный путь к файлу
2. PID процесса
3. TID потока
4. Файловый объект
 
