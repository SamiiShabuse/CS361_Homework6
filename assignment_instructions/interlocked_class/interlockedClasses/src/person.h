#ifndef _PERSON_H_
#define _PERSON_H_
#include <string>

//Tell the compiler hats are coming
#include "hat.h"
class Hat;

//A person has a name and can wear a hat.
class Person{
private:
	std::string name;
	Hat* myHat;
public:
	Person(std::string n);
	std::string getName();
	void addHat(Hat* h);
	void printAboutMe();
};

#endif
