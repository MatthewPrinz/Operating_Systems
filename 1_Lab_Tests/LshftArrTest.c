#include <stdio.h>
void printArr(int* arr, int len);
void lshftArray(int* arr, int len);

int main() {
    int tcA[7] = {1, 2, 3, 0, 5, 6, 7};
    lshftArray(tcA, 7);
    printArr(tcA, 7);
    int tcB[7] = {1, 2, 0, 0, 5, 6, 7};
    lshftArray(tcB, 7);
    printArr(tcB, 7);
    int tcC[6] = {0, 0, 1, 2, 3, 4};
    lshftArray(tcC, 6);
    printArr(tcC, 6);
    int tcD[20] = {1, 2, 3, 4, 0, 0, 0, 8, 9, 10, 0, 0, 13, 14, 0, 0, 17, 18, 19, 0};
    lshftArray(tcD, 20);
    printArr(tcD, 20);
    int tcE[20] = {0};
    tcE[0] = 1;
    lshftArray(tcE, 20);
    printArr(tcE, 20);
    int tcF[20] = {0};
    tcF[1] = 2;
    lshftArray(tcF, 20);
    printArr(tcF, 20);
}

void printArr(int* arr, int len)
{
    printf("\n");
    for (int i = 0; i < len; i++)
    {
        printf("%d: %d\n", i, arr[i]);
    }
}

void lshftArray(int* g_jobs, int len)
{
    for (int i = 0; i < len; i++)
    {
        // empty pos
        if (g_jobs[i] == 0)
        {
            int nearestFullElement;
            for (nearestFullElement = i; nearestFullElement < len-1; nearestFullElement++)
            {
                if (g_jobs[nearestFullElement] != 0)
                    break;
            }
            g_jobs[i] = g_jobs[nearestFullElement];
            g_jobs[nearestFullElement] = 0;
        }
    }
}