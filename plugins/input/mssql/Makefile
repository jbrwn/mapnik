ODBC_INCLUDE=-I/usr/local/include
ODBC_LIBPATH=-L/usr/local/lib 
ODBC_LIBNAME=-lodbc
MAPNIK_INCLUDE=-I../../../include -I../../../deps -I../../../deps/clipper/include -I../../../deps/agg/include -I../../../mason_packages/.link/include -I../../../mason_packages/.link/include/freetype2 -I../../../mason_packages/.link/include/libxml2 -I../../../mason_packages/.link/include/libpng16 -I../../../mason_packages/.link/include/gdal  -I../../../mason_packages/.link/include/cairo -I../../../mason_packages/.link/include/pixman-1
MAPNIK_LIBPATH=-L../../../src -L../../../src/json -L../../../src/wkt -L../../../mason_packages/.link/lib
MAPNIK_LIBNAME=-lmapnik -lboost_filesystem -lboost_regex -lcairo -lpixman-1 -lexpat -lpng -lproj -ltiff -lwebp -lxml2 -licui18n -lboost_system -lharfbuzz -ljpeg -licuuc -lfreetype -licudata -lz

CXX=clang++
COMPILE_FLAGS=-std=c++11 -stdlib=libc++ -fvisibility=hidden -fvisibility-inlines-hidden -DU_CHARSET_IS_UTF8=1 -Wno-c++11-narrowing -Wall -ftemplate-depth-300 -Wno-unknown-pragmas -Wno-unsequenced -O3 -fPIC
DEFINES=-DSHAPE_MEMORY_MAPPED_FILE -DMAPNIK_HAS_DLCFN -DBIGINT -DBOOST_REGEX_HAS_ICU -DHAVE_JPEG -DMAPNIK_USE_PROJ4 -DHAVE_PNG -DHAVE_WEBP -DHAVE_TIFF -DDARWIN -DMAPNIK_THREADSAFE -DBOOST_SPIRIT_NO_PREDEFINED_TERMINALS=1 -DBOOST_PHOENIX_NO_PREDEFINED_TERMINALS=1 -DBOOST_SPIRIT_USE_PHOENIX_V3=1 -DNDEBUG -DHAVE_CAIRO -DGRID_RENDERER -DSVG_RENDERER
CXXFLAGS=$(COMPILE_FLAGS) $(DEFINES) $(MAPNIK_INCLUDE) $(ODBC_INCLUDE)
LINK_FLAGS=-stdlib=libc++
LDFLAGS=$(LINK_FLAGS) $(MAPNIK_LIBPATH) $(ODBC_LIBPATH) $(MAPNIK_LIBNAME) $(ODBC_LIBNAME)

SRC = $(wildcard *.cpp)

OBJ = $(SRC:.cpp=.o)

BIN = ../mssql.input

all : $(SRC) $(BIN)

$(BIN) : $(OBJ)
	$(CXX) -shared $(OBJ) $(LDFLAGS) -o $@

.cpp.o :
	$(CXX) -c $(CXXFLAGS) $< -o $@

.PHONY : clean

clean:
	@echo $(CURDIR)
	rm -f $(OBJ)
	rm -f $(BIN)