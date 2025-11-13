#ifndef _HAT_H_
#define _HAT_H_
#include <string>
//Hats have a person's name on them
//tell the compiler Person will exist
#include "person.h"
class Person;

class Hat{
private:
	Person* myCreator;
public:
	Hat(Person* P);
	std::string info();
};

#endif
