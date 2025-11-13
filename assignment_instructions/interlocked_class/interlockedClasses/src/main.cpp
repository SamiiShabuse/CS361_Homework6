
#include "person.h"
#include "hat.h"

int main(int argv, char** argc){
	Person* A = new Person("Mike");
	Person* B = new Person("Ted");
	Hat* H = new Hat(B);
	A->addHat(H);
	A->printAboutMe();
	B->printAboutMe();
	return 0;
}
