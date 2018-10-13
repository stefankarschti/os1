#ifndef _TERMINAL_H_
#define _TERMINAL_H_

// C++ Terminal class

#include "stdbool.h"
#include "stdint.h"

class Terminal
{
public:
	Terminal(uint16_t *screen, int numRows, int numCols);
	void clear();
	void move(int row, int col);
	void write(const char* str);

private:
	uint16_t *_screen = nullptr;
	int _numRows = 0;
	int _numCols = 0;
	int _row = 0; 
	int _col = 0;
	
	void iput(char c);
};

#endif

