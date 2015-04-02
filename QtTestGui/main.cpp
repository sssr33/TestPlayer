#include "qttestgui.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[]){
	QApplication a(argc, argv);
	QtTestGui w;
	w.show();

	return a.exec();
}
