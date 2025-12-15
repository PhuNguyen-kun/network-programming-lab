#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>  
#include <arpa/inet.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>

typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;

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

void CreateHTML(const char* path, char** output)
{
    struct dirent** result = NULL;

    Append(output,"<html>");
    Append(output,"<head><style>");
    Append(output,"body { font-family: Arial; margin: 20px; }");
    Append(output,".upload-form { background: #f0f0f0; padding: 15px; border-radius: 5px; margin-bottom: 20px; }");
    Append(output,"input[type=file] { margin: 10px 0; }");
    Append(output,"input[type=submit] { background: #4CAF50; color: white; padding: 10px 20px; border: none; cursor: pointer; }");
    Append(output,"</style></head><body>");
    Append(output,"<h2>Current Directory: ");
    Append(output, path);
    Append(output,"</h2>");
    Append(output,"<div class=\"upload-form\">");
    Append(output,"<h3>Upload File</h3>");
    Append(output,"<form method=\"post\" enctype=\"multipart/form-data\">");
    Append(output,"<input type=\"file\" id=\"file1\" name=\"file1\" required/><br>");
    Append(output,"<input type=\"submit\" value=\"Upload to this directory\">");
    Append(output,"</form></div>");
    Append(output,"<h3>Directory Contents:</h3>");
    int n = scandir(path, &result, NULL, Compare);
    if (n > 0)
    {
        for (int i = 0;i < n;i++)
        {
            if (result[i]->d_type == DT_DIR)
            {
                Append(output,"<a href=\"");
                Append(output, path);
                if (path[strlen(path) - 1] != '/')
                {
                    Append(output,"/");
                }
                Append(output, result[i]->d_name);
                Append(output,"\">");
                Append(output,"<b>");
                Append(output,result[i]->d_name);
                Append(output,"</b>");
                Append(output,"</a>");
                Append(output,"<br>");
            }else
            {
                Append(output,"<a href=\"");
                Append(output, path);
                if (path[strlen(path) - 1] != '/')
                {
                    Append(output,"/");
                }
                Append(output, result[i]->d_name);
                Append(output, "?");
                Append(output,"\">");
                Append(output,"<i>");
                Append(output,result[i]->d_name);
                Append(output,"</i>");
                Append(output,"</a>");
                Append(output,"<br>");
            }
            free(result[i]);
            result[i] = NULL;
        }
    }

    Append(output,"</body></html>");
}

void url_decode(char* dst, const char* src)
{
    char a, b;
    while (*src)
    {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b)))
        {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

void Send(int c, char* data, int len)
{
    int sent = 0;
    while (sent < len)
    {
        int tmp = send(c, data + sent, len - sent, 0);
        if (tmp > 0)
        {
            sent += tmp;
        }else
            break;
    }
}

void* ClientThread(void* arg)
{
    int c = *((int*)arg);
    free(arg);
    arg = NULL;
    char* requestBody = NULL;
    char* partBody = NULL;
    int bodyLen = 0;
    int partLen = 0;
    while (requestBody == NULL || strstr(requestBody,"\r\n\r\n") == NULL)
    {
        char chr;
        int r = recv(c, &chr, 1, 0);
        if (r > 0)
        {
            requestBody = (char*)realloc(requestBody, bodyLen + 1);
            requestBody[bodyLen] = chr;
            bodyLen++;
        }else
            break;
    }
    
    if (requestBody != NULL && strstr(requestBody,"\r\n\r\n") != NULL)
    {
        int contentRead = 0;
        int contentLength = 0;
        char* ctLen = strstr(requestBody, "Content-Length: ");
        if (ctLen != NULL)
        {
            sscanf(ctLen + strlen("Content-Length: "), "%d", &contentLength);
        }
        printf("%s", requestBody);
        char method[8] = { 0 };
        char path[1024] = { 0 };
        char decoded_path[1024] = { 0 };
        sscanf(requestBody,"%s%s", method, path);
        url_decode(decoded_path, path);
        strcpy(path, decoded_path);

        if (strcmp(method,"GET") == 0)
        {
            if (strstr(path,"favicon.ico") != NULL)
            {
                FILE* f = fopen("favicon.ico","rb");
                if (f != NULL)
                {
                    fseek(f, 0, SEEK_END);
                    int length = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    char* icoData = (char*)calloc(length, 1);
                    fread(icoData, 1, length, f);
                    fclose(f);
                    char ok[1024] = { 0 };
                    sprintf(ok, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: image/x-icon\r\n\r\n", length);
                    Send(c, ok, strlen(ok));
                    Send(c, icoData, length);
                }else
                {
                    char* notFound = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
                    Send(c, notFound, strlen(notFound));
                }
            }else
            {
                if (path[strlen(path) - 1] != '?')
                {
                    char* html = NULL;
                    CreateHTML(path, &html);
                    char ok[1024] = { 0 };
                    sprintf(ok, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", (int)strlen(html));
                    Send(c, ok, strlen(ok));
                    Send(c, html, strlen(html));
                }else
                {
                    path[strlen(path) - 1] = 0;
                    
                    char response[1024] = { 0 };
                    FILE* f = fopen(path,"rb");
                    if (f != NULL)
                    {
                        fseek(f, 0, SEEK_END);
                        int len = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        
                        char* filename = strrchr(path, '/');
                        if (filename == NULL) filename = path;
                        else filename++;
                        
                        sprintf(response,"HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=\"%s\"\r\n\r\n", len, filename);
                        Send(c, response, strlen(response));
                        
                        int block_size = 1024 * 1024;
                        char*data = (char*)calloc(block_size, 1);
                        int read = 0;
                        while (read < len)
                        {
                            int tmpr = fread(data, 1, block_size, f);
                            Send(c, data, tmpr);
                            read += tmpr;
                        }
                        free(data);
                        data = NULL;
                        fclose(f);
                    }else
                    {
                        sprintf(response,"HTTP/1.1 404 NOT FOUND\r\n\r\n");
                        Send(c, response, strlen(response));
                    }
                }
            }
        }else  if (strcmp(method,"POST") == 0)
        {
            while (partBody == NULL || strstr(partBody,"\r\n\r\n") == NULL)
            {
                char chr;
                int r = recv(c, &chr, 1, 0);
                if (r > 0)
                {
                    partBody = (char*)realloc(partBody, partLen + 1);
                    partBody[partLen] = chr;
                    partLen++;
                    contentRead++;
                }else
                    break;
            }
            if (strstr(partBody,"\r\n\r\n") != NULL)
            {
                char* filename_start = strstr(partBody,"filename=\"");
                if (filename_start != NULL)
                {
                    filename_start += 10;
                    char* filename_end = strchr(filename_start, '"');
                    if (filename_end != NULL)
                    {
                        char name[1024] = { 0 };
                        int name_len = filename_end - filename_start;
                        strncpy(name, filename_start, name_len);
                        name[name_len] = 0;
                        
                        printf("Uploading file: %s to directory: %s\n", name, path);
                        
                        char fullpath[2048] = { 0 };
                        if (path[strlen(path) - 1] == '/')
                            sprintf(fullpath,"%s%s", path, name);
                        else
                            sprintf(fullpath,"%s/%s", path, name);
                        
                        FILE* f = fopen(fullpath, "wb");
                        if (f != NULL)
                        {
                            int total_written = 0;
                            char* boundary = strstr(requestBody, "boundary=");
                            char boundary_str[256] = { 0 };
                            if (boundary != NULL)
                            {
                                sscanf(boundary + 9, "%s", boundary_str);
                            }
                            
                            while (contentRead < contentLength)
                            {
                                char data[1024] = { 0 };
                                int r = recv(c, data, sizeof(data), 0);
                                if (r > 0)
                                {
                                    fwrite(data, 1, r, f);
                                    total_written += r;
                                    contentRead += r;
                                }else
                                    break;
                            }
                            fclose(f);
                            
                            FILE* verify = fopen(fullpath, "rb");
                            if (verify != NULL)
                            {
                                fseek(verify, 0, SEEK_END);
                                long file_size = ftell(verify);
                                fclose(verify);
                                
                                printf("File written: %s (size: %ld bytes)\n", fullpath, file_size);
                                
                                char response[1024] = { 0 };
                                char html_response[2048] = { 0 };
                                sprintf(html_response,
                                    "<html><body>"
                                    "<h2>Upload Successful!</h2>"
                                    "<p>File: <b>%s</b></p>"
                                    "<p>Size: <b>%ld</b> bytes</p>"
                                    "<p>Location: <b>%s</b></p>"
                                    "<a href=\"%s\">Back to directory</a>"
                                    "</body></html>",
                                    name, file_size, fullpath, path);
                                
                                sprintf(response,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s",
                                    (int)strlen(html_response), html_response);
                                Send(c, response, strlen(response));
                            }else
                            {
                                char* response = "HTTP/1.1 500 INTERNAL SERVER ERROR\r\nContent-Type: text/html\r\n\r\n<html><body><h2>Error: Could not verify uploaded file</h2></body></html>";
                                Send(c, response, strlen(response));
                            }
                        }else
                        {
                            char* response = "HTTP/1.1 500 INTERNAL SERVER ERROR\r\nContent-Type: text/html\r\n\r\n<html><body><h2>Error: Could not create file</h2></body></html>";
                            Send(c, response, strlen(response));
                        }
                    }else
                    {
                        char* response = "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: text/html\r\n\r\n<html><body><h2>Error: Invalid filename format</h2></body></html>";
                        Send(c, response, strlen(response));
                    }
                }else
                {
                    char* response = "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: text/html\r\n\r\n<html><body><h2>Error: No file selected</h2></body></html>";
                    Send(c, response, strlen(response));
                }
            }else
            {
                char* response = "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: text/html\r\n\r\n<html><body><h2>Error: Invalid request format</h2></body></html>";
                Send(c, response, strlen(response));
            }
        }
    }

    close(c);
}

int main()
{
    SOCKADDR_IN saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr("0.0.0.0");
    saddr.sin_port = htons(8888);
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (bind(s, (SOCKADDR*)&saddr, sizeof(SOCKADDR)) == 0)
    {
        listen(s, 10);
        while (0 == 0)
        {
            int c = accept(s, NULL, 0);
            pthread_t tid = 0;
            int* arg = (int*)calloc(1, sizeof(int));
            *arg = c;
            pthread_create(&tid, NULL, ClientThread, arg);
        }
    }else
    {
        printf("Failed to bind\n");
    }
}