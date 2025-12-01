#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

char* output = NULL;
char* root = "/home/phunguyen/IT/HUST/thuc-hanh-lap-trinh-mang";

void Append(char** destination, const char* str)
{
    char* tmp = *destination;

    int oldlen = tmp != NULL ? strlen(tmp) : 0;
    int more = strlen(str) + 1;
    tmp = (char*)realloc(tmp, oldlen + more);
    memset(tmp + oldlen, 0, more);
    sprintf(tmp + strlen(tmp), "%s", str);
    *destination = tmp;
}
int Compare(const struct dirent** e1, const struct dirent** e2)
{
    if ((*e1)->d_type == (*e2)->d_type)
    {
        return strcmp((*e1)->d_name, (*e2)->d_name);
    }else
    {
        if ((*e1)->d_type == DT_DIR)
            return -1;
        else
            return 1;
    }
}
int main()
{
    struct dirent** result = NULL;
    while (0 == 0)
    {
        Append(&output,"<html>");
        int n = scandir(root, &result, NULL, Compare);
        if (n > 0)
        {
            for (int i = 0;i < n;i++)
            {
                if (result[i]->d_type == DT_DIR)
                {
                    Append(&output,"<a href=\"");
                    Append(&output, result[i]->d_name);
                    Append(&output,"\">");
                    Append(&output,"<b>");
                    Append(&output,result[i]->d_name);
                    Append(&output,"</b>");
                    Append(&output,"</a>");
                    Append(&output,"<br>");
                }else
                {
                    Append(&output,"<a href=\"");
                    Append(&output, result[i]->d_name);
                    Append(&output,"\">");
                    Append(&output,"<i>");
                    Append(&output,result[i]->d_name);
                    Append(&output,"</i>");
                    Append(&output,"</a>");
                    Append(&output,"<br>");
                }
                free(result[i]);
                result[i] = NULL;
            }
        }
        Append(&output,"</html>");
        FILE*f = fopen("output.html","wb");
        fwrite(output, sizeof(char), strlen(output), f);
        fclose(f);
        free(output);
        output = NULL;
    }
}
