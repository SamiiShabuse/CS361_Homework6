#include "hat.h"

Hat::Hat(Person* P){
	myCreator = P;
}
std::string Hat::info(){
	return myCreator->getName();
}
