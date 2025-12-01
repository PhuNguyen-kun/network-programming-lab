#include <stdio.h>
#include <stdlib.h>

int main() {
    float *arr = NULL, num, sum = 0;
    int count = 0;

    while (1) {
        scanf("%f", &num);
        if (num == 0) break;
        float *tmp = (float*) realloc(arr, (count + 1) * sizeof(float));
        if (!tmp) { free(arr); return 1; }
        arr = tmp;
        arr[count++] = num;
    }
    
    for (int i = 0; i < count; i++) {
        sum += arr[i];
    }

    printf("%.6f\n", sum);

    free(arr);
    return 0;
}
