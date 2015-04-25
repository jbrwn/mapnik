# To use clang, run:  make CXX=clang++

ODBC_INCLUDES=-I/usr/local/include
ODBC_LIBS=-L/usr/local/lib -lodbc

CXXFLAGS = $(shell mapnik-config --cflags) $(ODBC_INCLUDES) -fPIC

LIBS = $(shell mapnik-config --libs --ldflags --dep-libs) $(ODBC_LIBS)

SRC = $(wildcard *.cpp)

OBJ = $(SRC:.cpp=.o)

BIN = mssql.input

all : $(SRC) $(BIN)

$(BIN) : $(OBJ)
	$(CXX) -shared $(OBJ) $(LIBS) -o $@

.cpp.o :
	$(CXX) -c $(CXXFLAGS) $< -o $@

.PHONY : clean

clean:
	rm -f $(OBJ)
	rm -f $(BIN)

deploy : all
	cp mssql.input $(shell mapnik-config --input-plugins)

install: all deploy
