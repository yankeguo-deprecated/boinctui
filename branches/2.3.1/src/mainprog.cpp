// =============================================================================
// This file is part of boinctui.
// http://boinctui.googlecode.com
// Copyright (C) 2012,2013 Sergey Suslov
//
// boinctui is free software; you can redistribute it and/or modify it  under
// the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// boinctui is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details
// <http://www.gnu.org/licenses/>.
// =============================================================================

#include <sys/ioctl.h>
#include <signal.h>
#include <sstream>
#include "kclog.h"
#include "mainprog.h"
#include "tuievent.h"
#include "nmessagebox.h"


#define EVTIMERINTERVAL 2 //число секунд через которые генерируется evTIMER


MainProg::MainProg()
{
    uistate = 0;
    done = false;
    cfg = new Config(".boinctui.cfg");
    gsrvlist = new SrvList(cfg);
    evtimertime = 0; //запускаем таймер с нуля
    //основное окно
    wmain 	= new MainWin(NRect(getmaxy(stdscr)-2, getmaxx(stdscr), 1, 0), cfg); //создаем основное окно
    insert(wmain);
    wmain->setserver(gsrvlist->getcursrv()); //отображать первый в списке сервер
    menu = new TopMenu(cfg);
    menu->setserver(gsrvlist->getcursrv()); //отображать первый в списке сервер
    insert(menu);
    setcaption();
    //статус строка
    wstatus 	= new NStaticText(NRect(1, getmaxx(stdscr), getmaxy(stdscr)-1, 0)); //создаем окно статуса
    insert(wstatus);
    wstatus->setbgcolor(getcolorpair(COLOR_WHITE,COLOR_GREEN));
    updatestatuslinecontent();
}


MainProg::~MainProg()
{
//    delete gsrvlist;
	cfg->save();
//    delete cfg;
}


void MainProg::setcaption()
{
    wmain->caption->clear();
    wmain->caption->append(getcolorpair(COLOR_WHITE,COLOR_BLACK) | A_BOLD," Host %s:%s ",gsrvlist->getcursrv()->gethost(),gsrvlist->getcursrv()->getport());
    wmain->refresh();
}


void MainProg::smartresize()
{
    if (!MainProg::needresize)
	return;
    struct winsize size;
    ioctl(fileno(stdout), TIOCGWINSZ, (char *) &size);
    resizeterm(size.ws_row, size.ws_col);
    menu->resize(1, getmaxx(stdscr)); //ширина верхнего меню
    wmain->resize(getmaxy(stdscr)-2, getmaxx(stdscr));
    wstatus->resize(1, getmaxx(stdscr)); //ширина статус строки
    wstatus->move(getmaxy(stdscr)-1,0); //позиция статус строки
    centermodalitems(getmaxy(stdscr),getmaxx(stdscr)); //центрировать модальные формы (если есть)
    MainProg::needresize = false;
}


void MainProg::updatestatuslinecontent()
{
    int attrYG = A_BOLD | getcolorpair(COLOR_YELLOW,COLOR_GREEN);
    int attrWG = A_BOLD | getcolorpair(COLOR_WHITE,COLOR_GREEN);
    int attrBG = A_BOLD | getcolorpair(COLOR_BLACK,COLOR_GREEN) | A_BOLD;
    if (uistate & stUIMODALFORM)
    {
	wstatus->setstring(attrYG, " Esc");
	wstatus->appendstring(attrWG, " Cancel");
    }
    if (uistate & stUITASKINFO)
    {
	wstatus->setstring(attrYG, " Esc");
	wstatus->appendstring(attrWG, " Cancel");
	wstatus->appendstring(attrWG, " |");
	wstatus->appendstring(attrYG, " PgUp/PgDn");
	wstatus->appendstring(attrWG, " Scroll ");
    }
    if ( (uistate == 0)||(uistate == stUISELECTOR) )
    {
	wstatus->setstring(attrYG, " PgUp/PgDn");
	wstatus->appendstring(attrWG, " Scroll Msg |");
	wstatus->appendstring(attrYG, " Up/Dn");
	wstatus->appendstring(attrWG, " Select |");
	if (uistate & stUISELECTOR)
	{
	    wstatus->appendstring(attrYG, " S");
	    wstatus->appendstring(attrWG, "uspend");
	    wstatus->appendstring(attrWG, " |");
	    wstatus->appendstring(attrYG, " R");
	    wstatus->appendstring(attrWG, "esume |");
	    wstatus->appendstring(attrYG, " Enter");
	    wstatus->appendstring(attrWG, " Info");
	}
	else
	{
	    wstatus->appendstring(attrBG, " Suspend");
	    wstatus->appendstring(attrWG, " |");
	    wstatus->appendstring(attrBG, " Resume");
	    wstatus->appendstring(attrWG, " |");
	    wstatus->appendstring(attrBG, " Enter Info");
	}
	wstatus->appendstring(attrWG, " |");
	wstatus->appendstring(attrYG, " F9");
	wstatus->appendstring(attrWG, " Menu |");
    }
}


void MainProg::eventhandle(NEvent* ev)	//обработчик событий КОРНЕВОЙ!
{
    NProgram::eventhandle(ev);
    if (ev->done) //если событие не обработано обработать здесь
	return;
    if (ev->type == NEvent::evKB) //клавиатурные
    {
        switch(ev->keycode)
	{
	    case 'q':
	    case 'Q':
		done = true; //выходим
		break;
	    case 'n':
	    case 'N':
		menu->disable();
		gsrvlist->nextserver();
		wmain->setserver(gsrvlist->getcursrv());
		menu->setserver(gsrvlist->getcursrv());
		evtimertime = 0; //для перезапуска таймера для форсированонй перерисовки
		setcaption();
		break;
	    case 'c':
	    case 'C':
		if (getitembyid(typeid(CfgForm).name()) == NULL)
		{
		    menu->disable();
		    CfgForm* cfgform = new CfgForm(15,54,cfg);
		    insert(cfgform);
		    cfgform->settitle("Configuration");
		    cfgform->refresh();
		    uistate = uistate | stUIMODALFORM;
		    updatestatuslinecontent();
		}
		break;
	    case 'S':
	    case 's':
	    {
		TaskInfo* tinfo = (TaskInfo*)wmain->wtask->getselectedobj();
		if (tinfo) //только если есть выделенный эл-т
		    gsrvlist->getcursrv()->optask(tinfo->projecturl.c_str(), tinfo->taskname.c_str(),"suspend_result");
		break;
	    }
	    case 'R':
	    case 'r':
	    {
		TaskInfo* tinfo = (TaskInfo*)wmain->wtask->getselectedobj();
		if (tinfo) //только если есть выделенный эл-т
		gsrvlist->getcursrv()->optask(tinfo->projecturl.c_str(), tinfo->taskname.c_str(),"resume_result");
		break;
	    }
	    case 'A':
	    case 'a':
	    {
		TaskInfo* tinfo = (TaskInfo*)wmain->wtask->getselectedobj();
		if (tinfo) //только если есть выделенный эл-т
		{
		    TuiEvent* ev = new TuiEvent(evABORTRES);
		    ev->bdata1 = false;
		    putevent(ev); //создаем событие с кодом 2 "abort_result"
		}
		break;
	    }
	    case 27:
		menu->disable();
		//деструктим все какие есть модельные окна
		destroybyid(typeid(CfgForm).name()); //деструктим форму
		destroybyid(typeid(NMessageBox).name()); //деструктим форму
		if (destroybyid(typeid(TaskInfoWin).name())) //деструктим форму
		{
		    wmain->wtask->setselectorenable(true);
		    uistate = uistate & ~stUITASKINFO;
		}
		uistate = uistate & ~stUIMODALFORM;
		updatestatuslinecontent();
		break;
	    case KEY_F(9):
		if (!menu->isenable())
		    menu->enable();
		else
		    menu->disable();
		break;
	    default:
		kLogPrintf("KEYCODE=%d\n", ev->keycode);
		break;
	} //switch
    }
    if (ev->type == NEvent::evPROG) //прграммные
    {
	switch(ev->cmdcode)
	{
	    case evCFGCH: //событие при изменении конфига
	    {
		menu->disable();
		destroybyid(typeid(CfgForm).name()); //деструктим форму
		//реакция на изменение конфига
		gsrvlist->refreshcfg();
		wmain->setserver(gsrvlist->getcursrv()); //отображать первый в списке сервер
		menu->setserver(gsrvlist->getcursrv()); //отображать первый в списке сервер
		setcaption();
		evtimertime = 0; //для перезапуска таймера для форсированонй перерисовки
		break;
	    }
	    case evABOUT: //событие About win
	    {
		if (!destroybyid(typeid(AboutWin).name()))
		{
		    AboutWin* about = new AboutWin(2,40);
		    insert(about);
		    about->move(getmaxy(stdscr)/2-about->getheight()/2,getmaxx(stdscr)/2-about->getwidth()/2); //центрируем
		    uistate = uistate | stUIMODALFORM;
		}
		else
		    uistate = uistate & ~stUIMODALFORM;
		updatestatuslinecontent();
		break;
	    }
	    case evKEYBIND: //событие KeyBinding win
	    {
		if (!destroybyid(typeid(HelpWin).name()))
		{
		    HelpWin* help = new HelpWin(2,40);
		    insert(help);
		    help->move(getmaxy(stdscr)/2-help->getheight()/2,getmaxx(stdscr)/2-help->getwidth()/2); //центрируем
		    uistate = uistate | stUIMODALFORM;
		}
		else
		    uistate = uistate & ~stUIMODALFORM;
		updatestatuslinecontent();
		break;
	    }
	    case evBENCHMARK: //запустить бенчмарк
	    {
		Srv* srv = gsrvlist->getcursrv();
		if (srv != NULL)
		    srv->runbenchmarks();
		break;
	    }
	    case evADDPROJECT: //добавить проект
	    {
		if (!destroybyid(typeid(AddProjectForm).name()))
		{
		    TuiEvent* ev1 = (TuiEvent*)ev;
		    Srv* srv = gsrvlist->getcursrv();
		    if (ev1->srv != NULL)
		    {
			AddProjectForm* addform = new AddProjectForm(30,65,ev1->srv,ev1->sdata1.c_str(),ev1->bdata1);
			insert(addform);
			addform->move(getmaxy(stdscr)/2-addform->getheight()/2,getmaxx(stdscr)/2-addform->getwidth()/2); //центрируем
			uistate = uistate | stUIMODALFORM;
		    }
		}
		else
		    uistate = uistate & ~stUIMODALFORM;
		updatestatuslinecontent();
		break;
	    }
	    case evADDACCMGR: //добавить акк менеджер
	    {
		if (!destroybyid(typeid(AddAccMgrForm).name()))
		{
		    TuiEvent* ev1 = (TuiEvent*)ev;
		    Srv* srv = gsrvlist->getcursrv();
		    if (ev1->srv != NULL)
		    {
			AddAccMgrForm* addmgrform = new AddAccMgrForm(30,65,ev1->srv,ev1->sdata1.c_str());
			insert(addmgrform);
			addmgrform->move(getmaxy(stdscr)/2-addmgrform->getheight()/2,getmaxx(stdscr)/2-addmgrform->getwidth()/2); //центрируем
			uistate = uistate | stUIMODALFORM;
		    }
		}
		else
		    uistate = uistate & ~stUIMODALFORM;
		updatestatuslinecontent();
		break;
	    }
	    case evPROJECTOP: //операции над проектом
	    {
		TuiEvent* ev1 = (TuiEvent*)ev;
		const char* projname = ev1->sdata1.c_str();
		const char* projop = ev1->sdata2.c_str();
		if (!ev1->bdata1) //если нет флага подтвержденного события, то не выполняем а спрашиваем юзера
		{
		    menu->disable(); //выключаем меню
		    //создаем окно сообщения с подтверждением
		    std::stringstream s;
		    s << "Please Confirm\n\n" << "Project   : "<< projname << "\nOperation : " << projop;
		    NMessageBox* mbox = new NMessageBox(s.str().c_str());
		    TuiEvent* buttonYev = new TuiEvent(evPROJECTOP, ev1->srv, projname, projop); //событие для кнопки Y
		    buttonYev->bdata1 = true; //флаг подтвержденности
		    mbox->addbutton(new NMButton("Yes",buttonYev, 'Y','y',0));
		    NEvent* buttonNev = new NEvent(NEvent::evKB, 27); //событие для кнопки N
		    mbox->addbutton(new NMButton("No",buttonNev, 'N','n',27,0));
		    insert(mbox);
		    uistate = uistate | stUIMODALFORM;
		}
		else
		{
		    kLogPrintf("evPROJECT confirmed event detected\n");
		    ev1->srv->opproject(projname, projop); //выполняем действие
		    if (destroybyid(typeid(NMessageBox).name())) //удаляем окно подтверждения (если есть)
			uistate = uistate & ~stUIMODALFORM;
		}
		updatestatuslinecontent();
		break;
	    }
	    case evABORTRES: //событие действий над проектами "abort_result" и.т.д.
	    {
		TaskInfo* tinfo = (TaskInfo*)wmain->wtask->getselectedobj();
		if (tinfo) //только если есть выделенный эл-т
		{
		    TuiEvent* ev1 = (TuiEvent*)ev;
		    if (!ev1->bdata1) //если нет флага подтвержденного события, то не выполняем а спрашиваем юзера
		    {
			menu->disable(); //выключаем меню
			//создаем окно сообщения с подтверждением
			std::stringstream s;
			s << "Please Confirm\n\n" << "Task   : " << tinfo->taskname << "\nOperation : " << "Abort";
			NMessageBox* mbox = new NMessageBox(s.str().c_str());
			TuiEvent* buttonYev = new TuiEvent(evABORTRES); //событие для кнопки Y
			buttonYev->bdata1 = true; //флаг подтвержденности
			mbox->addbutton(new NMButton("Yes",buttonYev, 'Y','y',0));
			NEvent* buttonNev = new NEvent(NEvent::evKB, 27); //событие для кнопки N
			mbox->addbutton(new NMButton("No",buttonNev, 'N','n',27,0));
			insert(mbox);
			uistate = uistate | stUIMODALFORM;
		    }
		    else
		    {
			kLogPrintf("evABORTRES confirmed event detected\n");
			Srv* srv = gsrvlist->getcursrv();
			srv->optask(tinfo->projecturl.c_str(), tinfo->taskname.c_str(),"abort_result"); //выполняем действие
			if (destroybyid(typeid(NMessageBox).name())) //удаляем окно подтверждения (если есть)
			    uistate = uistate & ~stUIMODALFORM;
		    }
		    updatestatuslinecontent();
		    break;
		}
	    }
	    case evTASKSELECTORON:
	    {
		uistate = uistate | stUISELECTOR;
		updatestatuslinecontent();
		break;
	    }
	    case evTASKSELECTOROFF:
	    {
		uistate = uistate & ~stUISELECTOR;
		updatestatuslinecontent();
		break;
	    }
	    case evTASKINFO:
	    {
		TaskInfo* tinfo = (TaskInfo*)wmain->wtask->getselectedobj();
		if (tinfo) //только если есть выделенный эл-т
		{
		    wmain->wtask->setselectorenable(false);
		    TaskInfoWin* taskinfowin = new TaskInfoWin("Task Info Raw View", gsrvlist->getcursrv(), tinfo->projecturl.c_str(), tinfo->taskname.c_str());
		    insert(taskinfowin);
		    taskinfowin->move(getmaxy(stdscr)/2-taskinfowin->getheight()/2,getmaxx(stdscr)/2-taskinfowin->getwidth()/2); //центрируем
		    uistate = uistate | stUITASKINFO;
		    updatestatuslinecontent();
		}
	    }
	} //switch
    }
}


bool MainProg::mainloop() //основной цикл порождающий события
{
    sigset_t newset;
    sigemptyset(&newset);
    sigaddset(&newset, SIGWINCH); //маска для сигнала 
    if (cfg->isdefault) //если конфига нет то открыть форму
	putevent(new NEvent(NEvent::evKB, 'C')); //создаем событие иммитирующее нажатие 'C'
    do
    {
	//блокировка сигнала изменения окна SIGWINCH на время отрисовки (из-за нереентерабельности курсес)
	sigprocmask(SIG_BLOCK, &newset, 0);
	//если нужен ресайз - перерисовать полностью
	if (MainProg::needresize)
	{
	    smartresize();
	    refresh();
	    menu->refresh();
	    //wmain->erase();
	    wstatus->erase();
	    wmain->refresh();
	    wstatus->refresh();
	}
	//если настало время посылаем evTIMER
	if (time(NULL) - evtimertime > EVTIMERINTERVAL)
	{
	    NEvent* event = new NEvent(NEvent::evTIMER, 0); //создаем событие таймера
	    putevent(event); //отправить в очередь
	    time(&evtimertime);
	}
	//есть символ в буфере
	int ic;
	if ( (ic = getch()) != ERR ) //символ(ы) есть?
	{
	    NEvent* event = new NEvent(NEvent::evKB, ic); //создаем событие
	    putevent(event); //отправить в очередь
	}
	//есть события в очереди - выполняем
	while(!evqueue.empty())
	{
	    NEvent* event = evqueue.front(); //получить первое событие из очереди
	    evqueue.pop();
	    this->eventhandle(event); //отправить событие обработчику
	    delete event; //удаляем отработанное событие
	    //обновляем экран
	    update_panels();
	    doupdate(); //физически выполняет перерисовку
	}
	//разблокируем SIGWINCH
	sigprocmask(SIG_UNBLOCK, &newset, 0); 
    }
    while(!done);
}