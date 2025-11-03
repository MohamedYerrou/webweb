NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -fsanitize=address -g3

SRC = main.cpp \
	srcs/CGIHandler.cpp \
	srcs/Client.cpp \
	srcs/Server.cpp \
	srcs/Location.cpp \
	srcs/run_server.cpp \
	srcs/Request.cpp \
	srcs/Response.cpp \
	srcs/Utils.cpp \
	srcs/GET.cpp \
	srcs/DELETE.cpp \
	srcs/POST.cpp

OBJ = $(SRC:.cpp=.o)

all : $(NAME)

$(NAME) : $(OBJ)
	${CXX} ${CXXFLAGS} ${OBJ} -o ${NAME}

clean :
	rm -rf $(OBJ)

fclean : clean
	rm -rf $(NAME)

re : fclean all

.PHONY : all clean fclean re

.SECONDARY:
