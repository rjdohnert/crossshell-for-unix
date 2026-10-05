# Makefile for dos2unix and unix2dos (Windows native / CrossShell)

CXX = cl
CXXFLAGS = /nologo /std:c++17 /EHsc /W4 /O2 /permissive- /utf-8
LIBS = Shell32.lib

SOURCES = dos2unix.cpp app.cpp converter.cpp options.cpp reporter.cpp timestamp_helper.cpp
OBJS = $(SOURCES:.cpp=.obj)

TARGET = dos2unix.exe
ALIAS = unix2dos.exe

all: $(TARGET) $(ALIAS)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) /Fe:$@ $(SOURCES) $(LIBS)

$(ALIAS): $(TARGET)
	copy /y $(TARGET) $(ALIAS)

clean:
	del /q /f *.obj *.exe *.pdb *.ilk 2>nul
