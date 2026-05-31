#include <stdio.h>
#include <stdlib.h>
#include "lodepng.h"
#define ROI_COUNT 13
// Порог яркости.
// Все пиксели ярче этого значения считаем кандидатами
// на танкеры.
#define THRESHOLD 150
// Максимальный размер связной компоненты.
// Большие объекты — это шум, блики, берег и т.д.
#define MAX_COMPONENT_SIZE 100

// Структура ROI (Region Of Interest)
// Хранит прямоугольную область поиска.

typedef struct
{
    int x;
    int y;
    int w;
    int h;
} ROI;

// Загрузка PNG изображения.
// Возвращает массив RGBA пикселей.
unsigned char* load_png(const char* filename,
                        unsigned* width,
                        unsigned* height)
{
    unsigned char* image = NULL;

    unsigned error = lodepng_decode32_file(
        &image,
        width,
        height,
        filename
    );

    // Проверка ошибок чтения
    if(error)
    {
        printf("PNG read error %u: %s\n",
               error,
               lodepng_error_text(error));
    }

    return image;
}

// Функция вычисления одномерного индекса пикселя.
// Переводит координаты x,y в индекс массива.
int idx(int x, int y, int width)
{
    return y * width + x;
}

// Главная функция

int main(int argc, char** argv)
{
    
    // Проверяем аргументы командной строки.
    // Пользователь должен передать имя PNG файла.
    
    if(argc < 2)
    {
        printf("Usage: tanker_counter image.png\n");
        return 0;
    }

    const char* filename = argv[1];

    unsigned width;
    unsigned height;

    // Загружаем изображение.
    unsigned char* picture = load_png(
        filename,
        &width,
        &height
    );

    // Проверка успешности загрузки
    if(!picture)
    {
        return -1;
    }

    // Размер изображения в пикселях.
    int size = width * height;

    // Выделяем память под:
    //
    // gray     - grayscale изображение
    // mask     - бинарная маска ярких пикселей
    // visited  - массив посещенных пикселей
    // queue    - очередь для BFS
    // component- текущая компонента связности

    unsigned char* gray =
        (unsigned char*)malloc(size);

    unsigned char* mask =
        (unsigned char*)calloc(size, 1);

    unsigned char* visited =
        (unsigned char*)calloc(size, 1);

    int* queue =
        (int*)malloc(size * sizeof(int));

    int* component =
        (int*)malloc(size * sizeof(int));


    // Перевод изображения в grayscale.
    // Используем стандартную формулу:
    // Y = 0.299R + 0.587G + 0.114B

    int x, y;

    for(y = 0; y < (int)height; y++)
    {
        for(x = 0; x < (int)width; x++)
        {
            int p = 4 * idx(x, y, width);

            unsigned char r = picture[p + 0];
            unsigned char g = picture[p + 1];
            unsigned char b = picture[p + 2];

            gray[idx(x, y, width)] =
                (unsigned char)(0.299 * r +
                                0.587 * g +
                                0.114 * b);
        }
    }

    // ROI области поиска танкеров.

    ROI rois[13];

    // Первая область
    rois[0].x = 545;
    rois[0].y = 0;
    rois[0].w = 178;
    rois[0].h = 275;

    // Вторая область
    rois[1].x = 503;
    rois[1].y = 302;
    rois[1].w = 247;
    rois[1].h = 17;

    rois[2].x = 557; //Третья область
    rois[2].y = 178;
    rois[2].w = 139;
    rois[2].h = 125;

    rois[3].x = 716;//Четвертая область
    rois[3].y = 318;
    rois[3].w = 370;
    rois[3].h = 330;

    rois[4].x = 569;//Пятая область
    rois[4].y = 579;
    rois[4].w = 59;
    rois[4].h = 22;

    rois[5].x = 534;//Шестая область
    rois[5].y = 477;
    rois[5].w = 183;
    rois[5].h = 82;

    rois[6].x = 569;//Седьмая область
    rois[6].y = 558;
    rois[6].w = 59;
    rois[6].h = 22;

    rois[7].x = 551;//Восьмая область
    rois[7].y = 318;
    rois[7].w = 116;
    rois[7].h = 40;

    rois[8].x = 504;//Девятая область
    rois[8].y = 328;
    rois[8].w = 31;
    rois[8].h = 30;

    rois[9].x = 536;//Десятая область
    rois[9].y = 326;
    rois[9].w = 16;
    rois[9].h = 22;

    rois[10].x = 509;//Одиннадцатая область
    rois[10].y = 357;
    rois[10].w = 208;
    rois[10].h = 58;

    rois[11].x = 534;//Двенадцатая область
    rois[11].y = 414;
    rois[11].w = 183;
    rois[11].h = 64;

    rois[12].x = 287;//Тринадцатая  область
    rois[12].y = 21;
    rois[12].w = 69;
    rois[12].h = 86;
    

    // Создаем бинарную маску.
    // Если пиксель ярче THRESHOLD,
    // считаем его частью потенциального танкера.
    int r;

    for(r = 0; r < ROI_COUNT; r++)
    {
        ROI roi = rois[r];

        for(y = roi.y; y < roi.y + roi.h; y++)
        {
            for(x = roi.x; x < roi.x + roi.w; x++)
            {
                // Проверка выхода за границы
                if(x < 0 || y < 0 ||
                   x >= (int)width ||
                   y >= (int)height)
                    continue;

                int id = idx(x, y, width);

                // Threshold filtering
                if(gray[id] > THRESHOLD)
                {
                    mask[id] = 1;
                }
            }
        }
    }
    // Счетчик найденных танкеров.
    int tanker_count = 0;

    // Смещения соседей для 8-связности.
    int dx[8] = {-1,0,1,-1,1,-1,0,1};
    int dy[8] = {-1,-1,-1,0,0,1,1,1};

    // Поиск  компонент связности
    // Используем BFS (обход в ширину).
    for(y = 0; y < (int)height; y++)
    {
        for(x = 0; x < (int)width; x++)
        {
            int start = idx(x, y, width);

            // Пропускаем черные пиксели
            if(mask[start] == 0)
                continue;

            // Пропускаем уже обработанные
            if(visited[start])
                continue;

            // Инициализация BFS.
            
            int qhead = 0;
            int qtail = 0;

            int comp_size = 0;

            queue[qtail++] = start;

            visited[start] = 1;

            
            // BFS обход компоненты
            while(qhead < qtail)
            {
                int v = queue[qhead++];

                component[comp_size++] = v;

                int cx = v % width;
                int cy = v / width;

                int k;

                // Проверяем всех соседей
                for(k = 0; k < 8; k++)
                {
                    int nx = cx + dx[k];
                    int ny = cy + dy[k];

                    // Проверка границ
                    if(nx < 0 || ny < 0 ||
                       nx >= (int)width ||
                       ny >= (int)height)
                        continue;

                    int ni = idx(nx, ny, width);

                    // Если сосед принадлежит маске
                    // и еще не посещен
                    if(mask[ni] && !visited[ni])
                    {
                        visited[ni] = 1;

                        queue[qtail++] = ni;
                    }
                }
            }

            // Фильтрация по размеру.
            // Маленькие bright-компоненты
            // считаем танкерами.
            if(comp_size >= 1 &&
               comp_size <= MAX_COMPONENT_SIZE)
            {
                tanker_count++;
            }
        }
    }

    // Вывод итогового количества танкеров.

    printf("Tankers found: %d\n", tanker_count);

    // Освобождение памяти.

    free(gray);
    free(mask);
    free(visited);
    free(queue);
    free(component);
    free(picture);

    return 0;
}
