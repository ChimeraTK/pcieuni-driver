####### Compiler, tools and options

CC            = gcc
CXX           = g++
DEFINES       = 
CFLAGS        = -pipe -g -Wall -W -D_REENTRANT $(DEFINES)
CXXFLAGS      = -pipe -g -std=c++0x -Wall -Wextra -pedantic -W -D_REENTRANT $(DEFINES)
ifdef UPCIEDEV_INCLUDE
	CXXFLAGS += -I$(UPCIEDEV_INCLUDE)
endif
INCPATH       =  -I. 
LINK          = g++
LFLAGS        = -Wl,--no-as-needed -lrt
LIBS          = 
DEL_FILE      = rm -f


####### Files
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)
TARGET  = devtest


all: $(TARGET)

$(TARGET):  $(OBJECTS)  
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(OBJCOMP) $(LIBS)

clean:
	-$(DEL_FILE) $(OBJECTS)

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

