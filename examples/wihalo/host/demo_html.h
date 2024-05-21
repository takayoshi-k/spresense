#ifndef __DEMO_HTML_H
#define __DEMO_HTML_H

#define CRLF "\r\n"

#define EDGE_COLOR      "#000"
#define BASE_COLOR      "#fff"
#define CHAR_PIXW       "3px"

#define HTTP_404 "HTTP/1.1 404 OK" CRLF CRLF

#define HTTP_PAGE_HEADER "HTTP/1.1 200 OK" CRLF \
  "Content-Type: text/html" CRLF \
  "Connection: keep-alive" CRLF \
  "Content-Length: "

#define FONT_SZ  "font-size: 400%; "
#define FONT_MARGIN  "margin: 3px; padding: 5px; "
#define FONT_WEIGHT "font-weight: bold; "
#define FONT_COLOR(col) "color: " col ";"
#define FONT_EDGE(edge_col) \
      "text-shadow: "     CHAR_PIXW " " CHAR_PIXW " 0 " edge_col ", " \
                      "-" CHAR_PIXW " " CHAR_PIXW " 0 " edge_col ", " \
                      "-" CHAR_PIXW " -" CHAR_PIXW " 0 " edge_col ", " \
                          CHAR_PIXW " -" CHAR_PIXW " 0 " edge_col "; "

#define HTML_STYLE \
    "<style type=\"text/css\"> "  \
      "p.demotitle { " FONT_SZ FONT_WEIGHT FONT_MARGIN \
                       FONT_COLOR(EDGE_COLOR) FONT_EDGE(BASE_COLOR) \
      "}" \
      "body { background-image: url(\"/video\"); "  \
             "background-repeat: no-repeat;" \
             "background-size: cover; } "  \
    "</style> "

#define HTML_CONTENT  \
  "<!DOCTYPE html> "  \
    "<html lang=\"ja\"> " \
      "<head> " \
        "<meta charset=\"UTF-8\"> " \
        "<title>SPRESENSE Camera 画像</title> " \
        HTML_STYLE \
      "</head> "  \
      "<body> " \
        "<p class=\"demotitle\">SPRESENSEカメラ画像（隣のブースから） </p>"  \
      "</body> "  \
    "</html> " CRLF CRLF

#endif // __DEMO_HTML_H
