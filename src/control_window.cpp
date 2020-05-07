#include "control_window.h"
#include <iostream>
//#include "control.h"

ControlWindow::ControlWindow(session_t & session_, Ui::ControlWindow & ui_) : _session(session_), _ui(ui_)
{
    session_._updateListener.emplace_back([this](SessionUpdateType sut){
        if (sut == UPDATE_SESSION || sut == UPDATE_FRAME)
        {
            this->updateUiSignal(static_cast<int>(sut));
        }
    });
}

template <typename T>
bool safe_stof(T & value, const std::string& str)
{
    try {
        value = std::stof(str);
        return true;
    }
    catch (const std::invalid_argument& ia) {return false;}
    catch (const std::out_of_range& oor)    {return false;}
    catch (const std::exception& e)         {return false;}
}

template <typename T>
bool safe_stoi(T & value, const std::string& str)
{
    try {
        value = std::stoi(str);
        return true;
    }
    catch (const std::invalid_argument& ia) {return false;}
    catch (const std::out_of_range& oor)    {return false;}
    catch (const std::exception& e)         {return false;}
}

template <typename T>
bool safe_stof(T & value, const QString& str)
{
    return safe_stof(value, std::string(str.toUtf8().constData()));
}

template <typename T>
bool safe_stoi(T & value, const QString& str)
{
    return safe_stoi(value, std::string(str.toUtf8().constData()));
}

void ControlWindow::playForward()                         {_session._play = 1;}
void ControlWindow::playBackward()                        {_session._play = -1;}
void ControlWindow::playStop()                            {_session._play = 0;}
void ControlWindow::next()                                {_session._m_frame += _session._frames_per_step;  update_session(UPDATE_SESSION);}
void ControlWindow::prev()                                {_session._m_frame -= _session._frames_per_step;  update_session(UPDATE_SESSION);}
void ControlWindow::fov(int fov)                          {_session._fov = fov;                             update_session(UPDATE_SESSION);}
void ControlWindow::fov(QString const & fov)              {safe_stoi(_session._fov, fov);                   update_session(UPDATE_SESSION);}
void ControlWindow::showFlow(bool valid)                  {_session._show_flow = valid;                     update_session(UPDATE_SESSION);}
void ControlWindow::showRendered(bool valid)              {_session._show_raytraced = valid;                update_session(UPDATE_SESSION);}
void ControlWindow::showIndex(bool valid)                 {_session._show_index = valid;                    update_session(UPDATE_SESSION);}
void ControlWindow::showPosition(bool valid)              {_session._show_position = valid;                 update_session(UPDATE_SESSION);}
void ControlWindow::showDepth(bool valid)                 {_session._show_depth = valid;                    update_session(UPDATE_SESSION);}
void ControlWindow::positionShowCurser(bool               valid){_session._show_curser = valid;             update_session(UPDATE_SESSION);}
void ControlWindow::showArrows(bool valid)                {_session._show_arrows = valid;                   update_session(UPDATE_SESSION);}
void ControlWindow::past(int frames)                      {_session._diffbackward = -frames;                update_session(UPDATE_SESSION);}
void ControlWindow::past(QString const & frames)          {safe_stoi(_session._diffbackward, frames);       update_session(UPDATE_SESSION);}
void ControlWindow::future(int frames)                    {_session._diffforward = frames;                  update_session(UPDATE_SESSION);}
void ControlWindow::future(QString const & frames)        {safe_stoi(_session._diffforward, frames);        update_session(UPDATE_SESSION);}
void ControlWindow::smoothing(int frames)                 {_session._smoothing = frames;                    update_session(UPDATE_SESSION);}
void ControlWindow::smoothing(QString const & frames)     {safe_stoi(_session._smoothing        , frames);  update_session(UPDATE_SESSION);}
void ControlWindow::framesPerSecond(QString const & value){safe_stoi(_session._frames_per_second,  value);  update_session(UPDATE_SESSION);}
void ControlWindow::framesPerStep(QString const & value)  {safe_stoi(_session._frames_per_step  ,  value);  update_session(UPDATE_SESSION);}
void ControlWindow::preresolution(QString const & value)  {safe_stoi(_session._preresolution    ,  value);  update_session(UPDATE_SESSION);}
void ControlWindow::flowRotation(bool valid)              {_session._diffrot = valid;                       update_session(UPDATE_SESSION);}
void ControlWindow::flowTranslation(bool valid)           {_session._difftrans = valid;                     update_session(UPDATE_SESSION);}
void ControlWindow::flowObjects(bool valid)               {_session._diffobjects = valid;                   update_session(UPDATE_SESSION);}
void ControlWindow::frame(QString const & frame)          {safe_stoi(_session._m_frame,frame);              update_session(UPDATE_SESSION);}
void ControlWindow::updateShader()                        {_session._reload_shader = true;                  update_session(UPDATE_SESSION);}
void ControlWindow::realtime(bool valid)                  {_session._realtime = valid;                      update_session(UPDATE_SESSION);}
void ControlWindow::debug(bool valid)                     {_session._debug = valid;                         update_session(UPDATE_SESSION);}
void ControlWindow::approximated(bool valid)              {_session._approximated = valid;                  update_session(UPDATE_SESSION);}
void ControlWindow::depthMax(QString const & value)       {safe_stof(_session._depth_scale, value);         update_session(UPDATE_SESSION);}
void ControlWindow::renderedVisibility(bool valid)        {_session._show_rendered_visibility = valid;               update_session(UPDATE_SESSION);}
void ControlWindow::depthTesting(bool valid)              {_session._depth_testing = valid;                 update_session(UPDATE_SESSION);}

void ControlWindow::update_session(SessionUpdateType kind)
{
    this->updateUiFlag = true;
    _session.scene_update(kind);
    this->updateUiFlag = false;
}

void ControlWindow::animating(QString const & value)
{
    if (value == "Always")
    {
        _session._animating = REDRAW_ALWAYS;
    }
    else if (value == "Automatic")
    {
        _session._animating = REDRAW_AUTOMATIC;
    }
    else if (value == "Manual")
    {
        _session._animating = REDRAW_MANUAL;
    }
    else
    {
        throw std::runtime_error("Unknown Key " + std::string(value.toUtf8().constData()));
    }
    _session.scene_update(UPDATE_ANIMATING);
}
void ControlWindow::executeCommand()
{
    exec_env env(IO_UTIL::get_programpath());
    pending_task_t *pending = new pending_task_t(~PendingFlag(0));
    env._pending_tasks.emplace_back(pending);
    exec(_ui.executeText->text().toUtf8().constData(), env, std::cout, _session, *pending);
}

void ControlWindow::depthbuffer(QString const & depthstr)
{
    depthbuffer_size_t depth;
    if (depthstr == "16 bit")
    {
        depth = DEPTHBUFFER_16_BIT;
    }
    else if (depthstr == "24 bit")
    {
        depth = DEPTHBUFFER_24_BIT;
    }
    else if (depthstr == "32 bit")
    {
        depth = DEPTHBUFFER_32_BIT;
    }
    else
    {
        throw std::runtime_error("Illegal Argument");
    }
    _session._depthbuffer_size = depth;
    _session.scene_update(UPDATE_SESSION);
}

void ControlWindow::saveScreenshot(){
    size_t width;
    safe_stoi(width, _ui.screenshotWidth->text());
    size_t height;
    safe_stoi(height, _ui.screenshotHeight->text());
    std::string view = _ui.screenshotView->currentText().toUtf8().constData();
    bool prerendering = _ui.screenshotPrerendering->isChecked();
    viewtype_t viewtype;
    if (view == "Rendered")
    {
        viewtype = VIEWTYPE_RENDERED;
    }
    else if (view == "Flow")
    {
        viewtype = VIEWTYPE_FLOW;
    }
    else if (view == "Position")
    {
        viewtype = VIEWTYPE_POSITION;
    }
    else if (view == "Index")
    {
        viewtype = VIEWTYPE_INDEX;
    }
    else if (view == "Depth")
    {
        viewtype = VIEWTYPE_DEPTH;
    }
    else
    {
        throw std::runtime_error("Illegal Argument");
    }
    
    if (prerendering)
    {
        for (int i = 0; i < 6; ++i)
        {
            std::string filename = _ui.screenshotFilename->text().toUtf8().constData();
            auto pindex = filename.find_last_of(".");
            filename = filename.substr(0, pindex) + std::to_string(i) + filename.substr(pindex);
            auto f = std::async(std::launch::async, take_save_lazy_screenshot, filename, width, height, _ui.screenshotCamera->currentText().toUtf8().constData(), viewtype, true, i, std::ref(_session._scene));
            
            _mtx.lock();
            _pending_futures.push_back(std::move(f));
            _mtx.unlock();
        }
    }
    else
    {
        auto f = std::async(std::launch::async, take_save_lazy_screenshot, _ui.screenshotFilename->text().toUtf8().constData(), width, height, _ui.screenshotCamera->currentText().toUtf8().constData(), viewtype, true, std::numeric_limits<size_t>::max(), std::ref(_session._scene));
        
        _mtx.lock();
        _pending_futures.push_back(std::move(f));
        _mtx.unlock();
    }
}
void ControlWindow::updateUi(){updateUi(UPDATE_SESSION);}
void ControlWindow::updateUi(int kind){
    if (this->updateUiFlag)
    {
        return;
    }
    this->updateUiFlag = true;
    if (kind == UPDATE_FRAME)
    {
        _ui.lineEditFrame->setText(QString::number(_session._m_frame));
    }
    else if (kind == UPDATE_SESSION)
    {
        _ui.renderedShow->setChecked(_session._show_raytraced);
        _ui.flowShow->setChecked(_session._show_flow);
        _ui.depthShow->setChecked(_session._show_depth);
        _ui.flowArrowsShow->setChecked(_session._show_arrows);
        _ui.flowRotation->setChecked(_session._diffrot);
        _ui.flowTranslation->setChecked(_session._difftrans);
        _ui.flowObjects->setChecked(_session._diffobjects);
        _ui.positionShow->setChecked(_session._show_position);
        _ui.indexShow->setChecked(_session._show_index);
        _ui.positionShowCurser->setChecked(_session._show_curser);
        _ui.generalSmoothing->setValue(_session._smoothing);
        _ui.checkBoxDebug->setChecked(_session._debug);
        _ui.checkBoxApproximated->setChecked(_session._approximated);
        _ui.depthScaleText->setText(QString::number(_session._depth_scale));
        _ui.checkBoxDepthTesting->setChecked(_session._depth_testing);
        _ui.renderedVisibility->setChecked(_session._show_rendered_visibility);
        _ui.generalSmoothingText->setText(QString::number(_session._smoothing));
        _ui.generalFov->setValue(_session._fov);
        _ui.generalFovText->setText(QString::number(_session._fov));
        _ui.flowPast->setValue(-_session._diffbackward);
        _ui.flowPastText->setText(QString::number(_session._diffbackward));
        _ui.flowFuture->setValue(_session._diffforward);
        _ui.flowFutureText->setText(QString::number(_session._diffforward));
        _ui.lineEditFrame->setText(QString::number(_session._m_frame));
        _ui.performancePreresolution->setCurrentText(QString::number(_session._preresolution));
        _ui.screenshotCamera->clear();
        for (camera_t & cam : _session._scene._cameras)
        {
            _ui.screenshotCamera->addItem(QString(cam._name.c_str()));
        }
    }
    this->updateUiFlag = false;
}

void ControlWindow::redraw()
{
    _session.scene_update(UPDATE_REDRAW);
}

/*void test()
{
    QWidget* TextFinder::loadUiFile()
{
    QUiLoader loader;

    QFile file(":/forms/textfinder.ui");
    file.open(QFile::ReadOnly);

    QWidget *formWidget = loader.load(&file, this);
    file.close();

    return formWidget;
        ui_findButton = findChild<QPushButton*>("findButton");
    ui_textEdit = findChild<QTextEdit*>("textEdit");
    ui_lineEdit = findChild<QLineEdit*>("lineEdit");
}
}*/
