#
# Makefile
# Computer Networking Programing Assignments
#
#  Created by Phillip Romig on 4/3/12.
#  Copyright 2012 Colorado School of Mines. All rights reserved.
#

CXX = g++
LD = g++
CXXFLAGS = -g  -std=c++11 -DBOOST_LOG_DYN_LINK
LDFLAGS = -g 
#CXXFLAGS = -g -pthread -std=c++11 -DBOOST_LOG_DYN_LINK
#LDFLAGS = -g -pthread

#
# You should be able to add object files here without changing anything else
#
TARGET = GoBackN
OBJ_FILES = ${TARGET}.o main.o simulator.o
INC_FILES = ${TARGET}.h includes.h main.h simulator.h

#
# Any libraries we might need.
#
#LIBRARYS = -lbsd -lboost_regex -lboost_log_setup -lboost_log  -lboost_thread -lboost_system -lpthread 
LIBRARYS = -lboost_regex -lboost_log_setup -lboost_log  -lboost_thread -lboost_system -lpthread 


${TARGET}: ${OBJ_FILES}
	${LD} ${LDFLAGS} ${OBJ_FILES} -o $@ ${LIBRARYS}

%.o : %.cpp ${INC_FILES}
	${CXX} -c ${CXXFLAGS} -o $@ $<

#
# Please remember not to submit objects or binarys.
#
clean:
	rm -f core ${TARGET} ${OBJ_FILES}

#
# This might work to create the submission tarball in the formal I asked for.
#
submit:
	rm -f core ${TARGET} ${OBJ_FILES}
	mkdir `whoami`
	cp Makefile README.txt *.h *.cpp `whoami`
	tar zcf `whoami`.tgz `whoami`
