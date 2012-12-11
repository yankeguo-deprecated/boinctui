#include <sys/ioctl.h>
#include <signal.h>
#include "kclog.h"
#include "mainprog.h"


MainProg::MainProg()
{
    done = false;
    cfg = new Config(".boinctui.cfg");
    gsrvlist = new SrvList(cfg);
    //основное окно
    wmain 	= new MainWin(NRect(getmaxy(stdscr)-2, getmaxx(stdscr), 1, 0)); //создаем основное окно
    insert(wmain);
    wmain->setserver(gsrvlist->getcursrv()); //отображать первый в списке сервер
    menu = new TopMenu();
    menu->setserver(gsrvlist->getcursrv()); //отображать первый в списке сервер
    insert(menu);
    setcaption();
    //статус строка
    wstatus 	= new NStaticText(NRect(1, getmaxx(stdscr), getmaxy(stdscr)-1, 0)); //создаем окно статуса
    insert(wstatus);
    wstatus->setbgcolor(getcolorpair(COLOR_WHITE,COLOR_GREEN));
    wstatus->appendstring(A_BOLD| getcolorpair(COLOR_YELLOW,COLOR_GREEN), " PgUp/PgDn");
    wstatus->appendstring(A_BOLD,"-Scroll Msg |");
    wstatus->appendstring(A_BOLD | getcolorpair(COLOR_YELLOW,COLOR_GREEN)," Up/Dn");
    wstatus->appendstring(A_BOLD,"-Select |");
    wstatus->appendstring(A_BOLD | getcolorpair(COLOR_YELLOW,COLOR_GREEN)," S");
    wstatus->appendstring(A_BOLD,"uspend |");
    wstatus->appendstring(A_BOLD | getcolorpair(COLOR_YELLOW,COLOR_GREEN)," R");
    wstatus->appendstring(A_BOLD,"esume |");
    wstatus->appendstring(A_BOLD | getcolorpair(COLOR_YELLOW,COLOR_GREEN)," F9");
    wstatus->appendstring(A_BOLD," Menu |");
}


void MainProg::setcaption()
{
    wmain->caption->clear();
    wmain->caption->append(A_BOLD,"Host %s:%s",gsrvlist->getcursrv()->gethost(),gsrvlist->getcursrv()->getport());
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
    if (cfgform != NULL)
	cfgform->move(getmaxy(stdscr)/2-cfgform->getheight()/2,getmaxx(stdscr)/2-cfgform->getwidth()/2); //окно конфигурации (если есть)
    MainProg::needresize = false;
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
		setcaption();
		break;
	    case 'c':
	    case 'C':
		if (cfgform == NULL)
		{
		    menu->disable();
		    cfgform = new CfgForm(15,54,cfg);
		    insert(cfgform);
		    cfgform->settitle("Configuration");
		    cfgform->refresh();
		}
		break;
	    case 27:
		menu->disable();
		//деструктим форму
		if (cfgform != NULL)
		{
		    remove(cfgform);
		    delete cfgform;
		    cfgform = NULL;
		}
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
	if (ev->cmdcode == 1) //событие при изменении конфига
	{
	    menu->disable();
	    //деструктим форму
	    if (cfgform != NULL)
	    {
	        remove(cfgform);
	        delete cfgform;
	        cfgform = NULL;
	    }
	    //реакция на изменение конфига
	    gsrvlist->refreshcfg();
	    wmain->setserver(gsrvlist->getcursrv()); //отображать первый в списке сервер
	    menu->setserver(gsrvlist->getcursrv()); //отображать первый в списке сервер
	    setcaption();
	}
    }
}


bool MainProg::mainloop() //основной цикл порождающий события
{
    int takt = 0; //номер оборота цикла
    time_t updatetime; //время последней отрисовки
    sigset_t newset;
    sigemptyset(&newset);
    sigaddset(&newset, SIGWINCH); //маска для сигнала 
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
	//есть символ в буфере
	int ic;
	if ( (ic = getch()) != ERR ) //символ(ы) есть?
	{
	    NEvent* event = new NEvent(NEvent::evKB, ic); //создаем событие
	    putevent(event); //отправить в очередь
	}
	//нет событий в очереди
	if (evqueue.empty())
	{
	    if (time(NULL)-updatetime > 2) //перерисовывались меньше 2х секунд назад
	    {
		wmain->wtask->updatedata(); 	//запросить данные с сервера
		wmain->wtask->refresh();		//перерисовать окно
		//обновить окно сообщений
		//if ( (takt % 2) == 0 )
		//{
		    wmain->wmsg->updatedata(); 	//запросить данные с сервера
		    wmain->wmsg->refresh(); 		//перерисовать окно
		//}
		//обновить инф панель
		//if ( (takt % 2) == 0 )
		//{
		    wmain->panel1->updatedata(); 	//сформировать данные
		    wmain->panel1->refresh(); 		//перерисовать окно
		//}
		time(&updatetime);
	    }
	    //takt++;
	}
	else
	{
	    NEvent* event = evqueue.front(); //получить первое событие из очереди
	    evqueue.pop();
	    this->eventhandle(event); //отправить событие обработчику
	    delete event; //удаляем отработанное событие
	    //обновляем экран
	}
	update_panels();
	doupdate(); //физически выполняет перерисовку
	//разблокируем SIGWINCH
	sigprocmask(SIG_UNBLOCK, &newset, 0); 
    }
    while(!done);
}