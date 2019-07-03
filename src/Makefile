TARGET  := pitchDetect

ifdef JAVASCRIPT
TARGET := pitchDetect.html
endif

SRCS  := pitchDetect.cpp jackrecorder.cpp

LDFLAGS += -L../third/aquila/ -L../third/aquila/lib
CXXFLAGS += -I../third/aquila/ -I/usr/include/rtmidi

ifeq ($(UNAME_S), Darwin)
 	CXX=clang++
	LD=clang++
	CXXFLAGS += `pkg-config --cflags sndfile samplerate` -I`brew --prefix jack`/jack/include -I`brew --prefix rtmidi`/include
	LDFLAGS += -L`brew --prefix jack`/lib -L`brew --prefix rtmidi`/lib  -L`brew --prefix boost`/lib
	LIBS +=  `pkg-config --libs sndfile samplerate` -lboost -ljack -lrtmidi -lboost_program_options -lAquila -lOoura_fft
else
        CXXFLAGS += `pkg-config --cflags sndfile samplerate jack rtmidi` -I../third/aquila/
        LIBS +=  `pkg-config --libs sndfile samplerate jack rtmidi` -lboost_program_options -lAquila -lOoura_fft
endif


#precompiled headers
HEADERS := 

GCH     := ${HEADERS:.h=.gch}
OBJS    := ${SRCS:.cpp=.o} 
DEPS    := ${SRCS:.cpp=.dep} 

.PHONY: all release debug clean distclean 

all: release
release: ${TARGET}
debug: ${TARGET}
info: ${TARGET}
profile: ${TARGET}
hardcore: ${TARGET}

${TARGET}: ${OBJS} 
	${CXX} ${LDFLAGS} -o $@ $^ ${LIBS} 

${OBJS}: %.o: %.cpp %.dep ${GCH}
	${CXX} ${CXXFLAGS} -o $@ -c $< 

${DEPS}: %.dep: %.cpp Makefile 
	${CXX} ${CXXFLAGS} -MM $< > $@ 

${GCH}: %.gch: ${HEADERS} 
	${CXX} ${CXXFLAGS} -o $@ -c ${@:.gch=.h}

install:
	mkdir -p ${DESTDIR}/${PREFIX}/bin
	cp ${TARGET} ${DESTDIR}/${PREFIX}/bin

uninstall:
	rm ${DESTDIR}/${PREFIX}/bin/${TARGET}

clean:
	rm -f *~ ${DEPS} ${OBJS} ${GCH} ${TARGET} 

distclean: clean



