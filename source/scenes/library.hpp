#pragma once
#include <string>

void Library_init(void);
void Library_exit(void);
void Library_suspend(void);
void Library_resume(std::string arg);
void Library_draw(void);
