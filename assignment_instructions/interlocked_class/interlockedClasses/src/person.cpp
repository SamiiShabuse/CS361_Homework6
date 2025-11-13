#include <iostream>
#include "person.h"

Person::Person(std::string n){
	name=n;
}
std::string Person::getName(){
	return name;
}
void Person::addHat(Hat* h){
	myHat = h;
}
void Person::printAboutMe(){
	std::cout << name;
	if(myHat!=nullptr){
		std::cout << " has a hat with "
		<< myHat->info();
	}
	std::cout << std::endl;
}
