// ScanDir mở rộng
// Giả định thư mục sẽ nhập không chứ dấu cách hoặc ký tự đặc biệt

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

char* output = NULL;
char root[1024] = "/mnt/c";

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

int is_directory(const char* path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        return S_ISDIR(st.st_mode);
    }
    return 0;
}

void update_root_up()
{
    int len = strlen(root);
    if (len <= 1) return;
    
    for (int i = len - 1; i >= 0; i--)
    {
        if (root[i] == '/')
        {
            if (i == 0)
            {
                root[1] = '\0';
            }else
            {
                root[i] = '\0';
            }
            break;
        }
    }
}

void update_root_down(const char* folder)
{
    int len = strlen(root);
    if (root[len - 1] != '/')
    {
        strcat(root, "/");
    }
    strcat(root, folder);
}

int main()
{
    struct dirent** result = NULL;

    while (1)
    {
        printf("\nCurrent directory: %s\n", root);
        
        Append(&output,"<html>");
        Append(&output,"<h2>");
        Append(&output, root);
        Append(&output,"</h2>");
        
        int n = scandir(root, &result, NULL, Compare);
        if (n > 0)
        {
            for (int i = 0; i < n; i++)
            {
                if (strcmp(result[i]->d_name, ".") == 0)
                {
                    free(result[i]);
                    continue;
                }
                
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
            free(result);
            result = NULL;
        }

        Append(&output,"</html>");
        FILE* f = fopen("output.html","wb");
        fwrite(output, sizeof(char), strlen(output), f);
        fclose(f);
        printf("HTML output written to output.html\n");
        
        free(output);
        output = NULL;

        printf("Enter folder name (or '..' to go up, 'q' to quit): ");
        char input[256];
        if (scanf("%255s", input) != 1) break;
        
        if (strcmp(input, "q") == 0)
        {
            break;
        }
        else if (strcmp(input, "..") == 0)
        {
            update_root_up();
        }
        else
        {
            char test_path[1024];
            snprintf(test_path, sizeof(test_path), "%s/%s", root, input);
            
            if (is_directory(test_path))
            {
                update_root_down(input);
            }
            else
            {
                printf("'%s' is not a valid directory!\n", input);
            }
        }
    }
    
    printf("Program terminated.\n");
    return 0;
}
