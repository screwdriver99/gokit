#include "mainwindow.h"

#include <blewrapper/service.h>

#include "./ui_mainwindow.h"

#define DEBUG_FLAG false

#define max_battery_volt 4.2f

#define pokit_status_service "57D3A771-267C-4394-8872-78223E92AEC5"  // wrong doc (it mentions ..C4)
#define pokit_device_ch "6974F5E5-0E54-45C3-97DD-29E4B5FB0849"
#define pokit_status_ch "3dba36e1-6120-4706-8dfd-ed9c16e569b6"
#define pokit_flashled_ch "ec9bb1f3-05a9-4277-8dd0-60a7896f0d6e"
#define pokit_torch_ch "aaf3f6d5-43d4-4a83-9510-dff3d858d4cc"   // subscr
#define pokit_button_ch "8fe5b5a9-b5b4-4a7b-8ff2-87224b970f89"  // subscr

// to confirm:
#define pokit_multimeter_service "e7481d2f-5781-442e-bb9a-fd4e3441dadc"
#define pokit_multimeter_setting_ch "53dc9a7a-bc19-4280-b76b-002d0e23b078"
#define pokit_multimeter_reading_ch "047d3559-8bee-423a-b229-4417fa603b90"  // subsc

#define pokit_dso_service "1569801e-1425-4a7a-b617-a4f4ed719de6"
#define pokit_dso_setting_ch "a81af1b6-b8b3-4244-8859-3da368d2be39"
#define pokit_dso_metadata_ch "970f00ba-f46f-4825-96a8-153a5cd0cda9"
#define pokit_dso_reading_ch "98e14f8e-536e-4f24-b4f4-1debfed0a99e"

#define mm_update_interval 200u

#define BUF_FROM_STRUCT(STRUCT) ::blew::Buffer(&STRUCT, sizeof(STRUCT))
#define PRINT(str, ...) fprintf(stderr, str "\n", ##__VA_ARGS__)
#define DEBUG_BUFFER(ch, b)                                             \
    {                                                                   \
        uint8_t* debugptr = b.buffer;                                   \
        PRINT("ch %s, size %u", ch.toString().c_str(), b.size);         \
        for (int i = 0; i < b.size; i++) PRINT("%#04hhx", debugptr[i]); \
        PRINT("end");                                                   \
    }

#define DSO_H_DIVISION_N 5
#define DSO_V_DIVISION_N 3

//=============================================================================
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      blew::Central(false),
      ui(new Ui::MainWindow),
      m_peripheral(nullptr),
      m_scanTimer(this),
      m_mmrxTimer(this),
      m_dsorxTimer(this),
      m_dsoScale(1.0f),
      m_dsoCmd(DSOC_FallingEdge)
{
    ui->setupUi(this);

    ui->batteryIndicator->setRoundedBar();

    ui->mmvalue->setDigitsNum(6, 3);
    ui->mmvalue->setAutoMinus(false);

    ui->mmrangeSelector->setArrayDir(gui::AD_Vertical);
    ui->dsoRangeSelector->setArrayDir(gui::AD_Vertical);

    ui->torchButton->setAutoMode(false);
    ui->torchButton->setActiveText("ON");

    ui->dsotriggerButton->setAutoMode(false);
    ui->dsotriggerButton->setActiveText("RUNNING");

    gui::UltraEntry e{};
    e.text = "Multimeter";
    e.data = ui->stack_multimeterPage;
    e.id   = GDM_Multimeter;
    ui->deviceModeSelector->addEntry(e, true);

    e.text = "Oscilloscope";
    e.data = ui->stack_dsoPage;
    e.id   = GDM_Oscilloscope;
    ui->deviceModeSelector->addEntry(e);

    e.grayed = true;
    e.text   = "Datalogger";
    e.data   = ui->stack_dataloggerPage;
    e.id     = GDM_Datalogger;
    ui->deviceModeSelector->addEntry(e);
    //

    e = {};

    e.text = "Idle";
    e.id   = MM_IDLE;
    ui->mmModeSelector->addEntry(e);

    e.text = "DC Voltage";
    e.id   = MM_DCVoltage;
    ui->mmModeSelector->addEntry(e);

    e.text = "AC Voltage";
    e.id   = MM_ACVoltage;
    ui->mmModeSelector->addEntry(e);

    e.text = "DC Current";
    e.id   = MM_DCCurrent;
    ui->mmModeSelector->addEntry(e);

    e.text = "AC Current";
    e.id   = MM_ACCurrent;
    ui->mmModeSelector->addEntry(e);

    e.text = "Resistance";
    e.id   = MM_Resistance;
    ui->mmModeSelector->addEntry(e);

    e.text = "Diode";
    e.id   = MM_Diode;
    ui->mmModeSelector->addEntry(e);

    e.text = "Continuity";
    e.id   = MM_Continuity;
    ui->mmModeSelector->addEntry(e);

    e.text = "Temperature";
    e.id   = MM_Temperature;
    ui->mmModeSelector->addEntry(e);

    //

    e = {};

    e.text = "Falling edge";
    e.id   = DSOC_FallingEdge;
    ui->dsoModeSelector->addEntry(e, true);

    e.text = "Rising edge";
    e.id   = DSOC_RisingEdge;
    ui->dsoModeSelector->addEntry(e);

    e.text = "Continuos";
    e.id   = DSOC_Continuos;
    ui->dsoModeSelector->addEntry(e);

    e.text = "Free running";
    e.id   = DSOC_FreeRunning;
    ui->dsoModeSelector->addEntry(e);

    //

    e = {};

    e.text = "DC Voltage";
    e.id   = DOM_VDC;
    ui->dsoMeasureSelector->addEntry(e, true);

    e.text = "AC Voltage";
    e.id   = DOM_VAC;
    ui->dsoMeasureSelector->addEntry(e);

    e.text = "DC Current";
    e.id   = DOM_ADC;
    ui->dsoMeasureSelector->addEntry(e);

    e.text = "AC Current";
    e.id   = DOM_AAC;
    ui->dsoMeasureSelector->addEntry(e);

    //

    _setupMMRangeSelector(MM_IDLE);
    _setupDSORangeSelector(DOM_VDC);

    // clang-format off
    connect(ui->deviceModeSelector, SIGNAL(onClickForStacked(QWidget*)), ui->stackedWidget,
            SLOT(setCurrentWidget(QWidget*)));

    connect(ui->deviceModeSelector, SIGNAL(onClick(int32_t,void*)), this,
            SLOT(_onDeviceModeChange(int32_t,void*)));

    connect(ui->mmModeSelector, SIGNAL(onClick(int32_t,void*)), this,
            SLOT(_onMultimeterModeChange(int32_t,void*)));

    connect(ui->dsoModeSelector, SIGNAL(onClick(int32_t,void*)), this,
            SLOT(_onDSOModeChange(int32_t,void*)));

    connect(ui->dsoMeasureSelector, SIGNAL(onClick(int32_t,void*)), this,
            SLOT(_onDSOMeasureChange(int32_t,void*)));

    connect(ui->mmrangeSelector, SIGNAL(onClick(const gui::UltraEntry*)), this,
            SLOT(_onMultimeterRangeSelectorPress(const gui::UltraEntry*)));

    connect(ui->dsoRangeSelector, SIGNAL(onClick(const gui::UltraEntry*)), this,
            SLOT(_onDSORangeSelectorPress(const gui::UltraEntry*)));

    connect(ui->scanButton, SIGNAL(onClick()), this, SLOT(_onScanButtonClick()));

    connect(&m_scanTimer, SIGNAL(timeout()), this, SLOT(_onScanTimerTimeout()));
    connect(&m_mmrxTimer, SIGNAL(timeout()), this, SLOT(_onMMRxTimerTimeout()));
    connect(&m_dsorxTimer, SIGNAL(timeout()), this, SLOT(_onDSORxTimerTimeout()));

    connect(ui->connectionSelector, SIGNAL(onPick(const gui::UltraEntry*)), this,
            SLOT(_deviceSelected(const gui::UltraEntry*)));

    connect(ui->connectButton, SIGNAL(onClick()), this, SLOT(_onConnectButtonClick()));

    connect(ui->torchButton, SIGNAL(onChange(bool)), this, SLOT(_onTorchButtonChange(bool)));

    connect(ui->dsotriggerButton, SIGNAL(onChange(bool)), this, SLOT(_onDsoTriggerButtonChange(bool)));
    // clang-format on

    m_mmrxTimer.setSingleShot(true);
    m_mmrxTimer.setInterval(500);

    m_dsorxTimer.setSingleShot(true);
    m_dsorxTimer.setInterval(500);
}
//=============================================================================
MainWindow::~MainWindow() { delete ui; }
//=============================================================================
MultimeterMode MainWindow::_currentMMMode()
{
    if (ui->mmModeSelector->current())
        return (MultimeterMode)ui->mmModeSelector->current()->id;
    else
        return MM_IDLE;
}
//=============================================================================
uint8_t MainWindow::_currentMMRange()
{
    if (ui->mmrangeSelector->current())
        return (uint8_t)ui->mmrangeSelector->current()->id;
    else
        return 255;  // autorange
}
//=============================================================================
uint8_t MainWindow::_currentDSORange()
{
    if (ui->dsoRangeSelector->current())
        return (uint8_t)ui->dsoRangeSelector->current()->id;
    else
        return 0;
}
//=============================================================================
DSOCommand MainWindow::_currentDSOCommand()
{
    if (ui->dsoModeSelector->current())
        return (DSOCommand)ui->dsoModeSelector->current()->id;
    else
        return DSOC_FallingEdge;
}
//=============================================================================
DSOOpMode MainWindow::_currentDSOMode()
{
    if (ui->dsoMeasureSelector->current())
        return (DSOOpMode)ui->dsoMeasureSelector->current()->id;
    else
        return DOM_Idle;
}
//=============================================================================
QString MainWindow::_modeswToStr(ModeSwitchPosition mode)
{
    switch (mode)
    {
        case MSP_Voltage:
            return "Voltage";
        case MSP_Mixed:
            return "Mixed";
        case MSP_Current:
            return "Current";
    }

    return "";
}
//=============================================================================
void MainWindow::_updateDevData(const DeviceData& data)
{
    ui->firmwareVerLabel->setText(QString("%1.%2").arg(data.fwMaj).arg(data.fwMin));
    ui->maxVoltLabel->setText(QString("%1 V").arg(data.maxVoltage));
    ui->maxCurrentLabel->setText(QString("%1 A").arg(data.maxCurrent));
    ui->maxResistanceLabel->setText(QString("%1 KOhm").arg(data.maxResistance));
    ui->maxSampleRateLabel->setText(QString("%1 KHz").arg(data.maxSamplingRate));
    ui->bufSizeLabel->setText(QString("%1").arg(data.maxBufferSize));
    ui->macAddrLabel->setText(QString("%1:%2:%3:%4:%5:%6")
                                  .arg((ushort)data.macAddr[0], 2, 16, (QChar)'0')
                                  .arg((ushort)data.macAddr[1], 2, 16, (QChar)'0')
                                  .arg((ushort)data.macAddr[2], 2, 16, (QChar)'0')
                                  .arg((ushort)data.macAddr[3], 2, 16, (QChar)'0')
                                  .arg((ushort)data.macAddr[4], 2, 16, (QChar)'0')
                                  .arg((ushort)data.macAddr[5], 2, 16, (QChar)'0'));
}
//=============================================================================
void MainWindow::_updateDevStatus(const DeviceStatus& status)
{
    QString stateStr;

    switch (status.state)
    {
        case DS_MM_Idle:
            stateStr = "Idle";
            break;
        case DS_MM_DCVoltage:
            stateStr = "MM DC Voltage";
            break;
        case DS_MM_ACVoltage:
            stateStr = "MM AC Voltage";
            break;
        case DS_MM_DCCUrrent:
            stateStr = "MM DC Current";
            break;
        case DS_MM_ACCurrent:
            stateStr = "MM AC Current";
            break;
        case DS_MM_Resistance:
            stateStr = "MM Resistance";
            break;
        case DS_MM_Diode:
            stateStr = "MM Diode";
            break;
        case DS_MM_Continuity:
            stateStr = "MM Continuity";
            break;
        case DS_MM_Temperature:
            stateStr = "MM Temperature";
            break;
        case DS_DSO:
            stateStr = "Oscilloscope";
            break;
        case DS_Datalogger:
            stateStr = "Datalogger";
            break;
    }

    ui->statusLabel->setText(stateStr);

    ui->batteryIndicator->setProgressBar(int((status.batteryVoltage / max_battery_volt) * 1000.0f));
    ui->batteryLabel->setText(QString("%1V").arg(status.batteryVoltage));

    ui->modeSwitchLabel->setText(_modeswToStr(status.modeswitch));
    _setupMMModeSelector(status.modeswitch);

#if DEBUG_FLAG == true
    PRINT("spare 0 %#04hhx", status.spare0);
    PRINT("spare 1 %#04hhx", status.spare1);
#endif
}
//=============================================================================
void MainWindow::_updateDeviceMMMode()
{
    if (!m_peripheral) return;
    auto c = m_peripheral->characteristic_r(pokit_multimeter_setting_ch);
    if (!c) return;

    MMSettings mmsettings = {};

    mmsettings.mode           = _currentMMMode();
    mmsettings.range          = _currentMMRange();
    mmsettings.updateInterval = mm_update_interval;

    c->writeValue(BUF_FROM_STRUCT(mmsettings));
}
//=============================================================================
void MainWindow::_updateDeviceDSOMode(bool stop)
{
    if (!m_peripheral) return;
    auto c = m_peripheral->characteristic_r(pokit_dso_setting_ch);
    if (!c) return;

    DSOSettings settings = {};

    settings.command = _currentDSOCommand();
    settings.trigger = 0.0f;  // to set
    settings.mode    = stop ? DOM_Idle : _currentDSOMode();
    settings.range   = _currentDSORange();
    settings.window  = 100000;  // to set
    settings.samples = 1000;    // to set

    PRINT("command %u", settings.command);
    PRINT("trigger %f", settings.trigger);
    PRINT("mode %u", settings.mode);
    PRINT("range %u", settings.range);
    PRINT("window %u", settings.window);
    PRINT("samples %u", settings.samples);

    c->writeValue(BUF_FROM_STRUCT(settings));
}
//=============================================================================
void MainWindow::_setupMMModeSelector(ModeSwitchPosition sw)
{
    ui->mmModeSelector->setAllGrayed(false);

    switch (sw)
    {
        case MSP_Voltage:
            ui->mmModeSelector->setGrayed(MM_DCCurrent);
            ui->mmModeSelector->setGrayed(MM_ACCurrent);
            ui->mmModeSelector->setGrayed(MM_Resistance);
            ui->mmModeSelector->setGrayed(MM_Diode);
            ui->mmModeSelector->setGrayed(MM_Continuity);
            break;
        case MSP_Mixed:
            ui->mmModeSelector->setGrayed(MM_DCVoltage);
            ui->mmModeSelector->setGrayed(MM_ACVoltage);
            break;
        case MSP_Current:
            ui->mmModeSelector->setGrayed(MM_DCVoltage);
            ui->mmModeSelector->setGrayed(MM_ACVoltage);
            ui->mmModeSelector->setGrayed(MM_Resistance);
            ui->mmModeSelector->setGrayed(MM_Diode);
            ui->mmModeSelector->setGrayed(MM_Continuity);
            break;
    }
}
//=============================================================================
void MainWindow::_setupMMRangeSelector(MultimeterMode mode)
{
    ui->mmrangeSelector->clear();

    switch (mode)
    {
        case MM_DCVoltage:
        case MM_ACVoltage:
            ui->rangeSetupFrame->setVisible(true);
            ui->mmrangeSelector->addButton({"0~300mV", 0});
            ui->mmrangeSelector->addButton({"300mV~2V", 1});
            ui->mmrangeSelector->addButton({"2V~6V", 2});
            ui->mmrangeSelector->addButton({"6V~12V", 3});
            ui->mmrangeSelector->addButton({"12V~30V", 4});
            ui->mmrangeSelector->addButton({"30V~60V", 5});
            ui->mmrangeSelector->addButton({"AUTO", 255});
            break;

        case MM_DCCurrent:
        case MM_ACCurrent:
            ui->rangeSetupFrame->setVisible(true);
            ui->mmrangeSelector->addButton({"0~10mA", 0});
            ui->mmrangeSelector->addButton({"10mA~30mA", 1});
            ui->mmrangeSelector->addButton({"30mA~150mA", 2});
            ui->mmrangeSelector->addButton({"150mA~300mA", 3});
            ui->mmrangeSelector->addButton({"300mA~3A", 4});
            ui->mmrangeSelector->addButton({"AUTO", 255});
            break;

        case MM_Resistance:
            ui->rangeSetupFrame->setVisible(true);
            ui->mmrangeSelector->addButton({"0~160R", 0});
            ui->mmrangeSelector->addButton({"160R~330R", 1});
            ui->mmrangeSelector->addButton({"330R~890R", 2});
            ui->mmrangeSelector->addButton({"890R~1K5", 3});
            ui->mmrangeSelector->addButton({"1K5~10KOhm", 4});
            ui->mmrangeSelector->addButton({"10K~100K", 5});
            ui->mmrangeSelector->addButton({"100R~470K", 6});
            ui->mmrangeSelector->addButton({"470R~1M", 7});
            ui->mmrangeSelector->addButton({"AUTO", 255});
            break;

        default:
            ui->rangeSetupFrame->setVisible(false);
            break;
    }
}
//=============================================================================
void MainWindow::_setupDSORangeSelector(DSOOpMode mode)
{
    ui->dsoRangeSelector->clear();

    switch (mode)
    {
        case DOM_Idle:
            ui->dsoRangeSelector->hide();
            return;

        case DOM_VDC:
        case DOM_VAC:
            // voltage
            ui->dsoRangeSelector->addButton({"0~300mV", 0}, true);
            ui->dsoRangeSelector->addButton({"300mV~2V", 1});
            ui->dsoRangeSelector->addButton({"2V~6V", 2});
            ui->dsoRangeSelector->addButton({"6V~12V", 3});
            ui->dsoRangeSelector->addButton({"12V~30V", 4});
            ui->dsoRangeSelector->addButton({"30V~60V", 5});
            break;

        case DOM_ADC:
        case DOM_AAC:
            // current
            ui->dsoRangeSelector->addButton({"0~10mA", 0}, true);
            ui->dsoRangeSelector->addButton({"10mA~30mA", 1});
            ui->dsoRangeSelector->addButton({"30mA~150mA", 2});
            ui->dsoRangeSelector->addButton({"150mA~300mA", 3});
            ui->dsoRangeSelector->addButton({"300mA~3A", 4});
            break;
    }

    ui->dsoRangeSelector->show();
}
//=============================================================================
void MainWindow::_setupDSOOscilloscope(const DSOMetadata& metadata)
{
    float time       = metadata.window / 1000.0f;  // in milliseconds
    float samplesize = time / metadata.samples;

    ui->oscilloscope->clear();
    ui->oscilloscope->setHorizontalScale(time / (float)DSO_H_DIVISION_N, DSO_H_DIVISION_N, samplesize);
    ui->oscilloscope->setVerticalScale(
        _dsorangeToMax(metadata.mode, metadata.range) / (float)DSO_V_DIVISION_N, DSO_V_DIVISION_N);
}
//=============================================================================
QString MainWindow::_mmrangeToStr(uint8_t range, MultimeterMode mode)
{
    if (mode == MM_DCVoltage || mode == MM_DCVoltage) switch (range)
        {
            case 0:
                return "0~300mV";
            case 1:
                return "300mV~2V";
            case 2:
                return "2V~6V";
            case 3:
                return "6V~12V";
            case 4:
                return "12V~30V";
            case 5:
                return "30V~60V";
            case 255:
                return "AUTO";
        }

    else if (mode == MM_DCCurrent || mode == MM_ACCurrent)
        switch (range)
        {
            case 0:
                return "0~10mA";
            case 1:
                return "10mA~30mA";
            case 2:
                return "30mA~150mA";
            case 3:
                return "150mA~300mA";
            case 4:
                return "300mA~3A";
            case 255:
                return "AUTO";
        }

    else if (mode == MM_Resistance)
        switch (range)
        {
            case 0:
                return "0R~160R";
            case 1:
                return "160R~330R";
            case 2:
                return "330R~890R";
            case 3:
                return "890R~1K5";
            case 4:
                return "1K5~10K";
            case 5:
                return "10K~100K";
            case 6:
                return "100K~470K";
            case 7:
                return "470K~1M";
            case 255:
                return "AUTO";
        }

    return "---";
}
//=============================================================================
QString MainWindow::_mmmodeToStr(MultimeterMode mode)
{
    switch (mode)
    {
        case MM_IDLE:
            return "Idle";
        case MM_DCVoltage:
            return "DC Voltage";
        case MM_ACVoltage:
            return "AC Voltage";
        case MM_DCCurrent:
            return "DC Current";
        case MM_ACCurrent:
            return "AC Current";
        case MM_Resistance:
            return "Resistance";
        case MM_Diode:
            return "Diode";
        case MM_Continuity:
            return "Continuity";
        case MM_Temperature:
            return "Temperature";
    }

    return "---";
}
//=============================================================================
float MainWindow::_dsorangeToMax(DSOOpMode mode, uint8_t range)
{
    switch (mode)
    {
        case DOM_VDC:
        case DOM_VAC:
            switch (range)
            {
                case 0:
                    return 0.3f;
                case 1:
                    return 2.0f;
                case 2:
                    return 6.0f;
                case 3:
                    return 12.0f;
                case 4:
                    return 30.0f;
                case 5:
                    return 60.0f;
            }
        case DOM_ADC:
        case DOM_AAC:
            switch (range)
            {
                case 0:
                    return 0.01f;
                case 1:
                    return 0.03f;
                case 2:
                    return 0.15f;
                case 3:
                    return 0.3f;
                case 4:
                    return 3.0f;
            }

        default:
            break;
    }

    return 1.0f;
}
//=============================================================================
QString MainWindow::_dsorangeToStr(DSOOpMode mode, uint8_t range)
{
    switch (mode)
    {
        case DOM_Idle:
            return "Idle";
        case DOM_VDC:
        case DOM_VAC:
            return "AC Voltage";
        case DOM_ADC:
            return "DC Current";
        case DOM_AAC:
            return "AC Current";
    }
    return "---";
}
//=============================================================================
void MainWindow::_updateMMLeds(MultimeterMode mode, uint8_t status)
{
    bool autorange  = false;
    bool continuity = false;

    switch (mode)
    {
        case MM_DCVoltage:
        case MM_ACVoltage:
        case MM_DCCurrent:
        case MM_ACCurrent:
        case MM_Resistance:
            autorange = status == 1;
            break;
        case MM_Continuity:
            continuity = status == 1;
            break;
        default:
            break;
    }

    ui->mmautorangeLed->activate(autorange);
    ui->mmerrorLed->activate(status == 255);
    ui->mmcontinuityLed->activate(continuity);
}
//=============================================================================
void MainWindow::_dsoReading(const DSOReading& data, uint32_t size)
{
    ui->dsotriggerButton->setState(true);
    m_dsorxTimer.start();

    std::vector<float> v(size);
    for (uint32_t i = 0; i < size; i++) v[i] = data.data[i] * m_dsoScale;
    ui->oscilloscope->addBlock(v.data(), size);
}
//=============================================================================
void MainWindow::_dsoMetadata(const DSOMetadata& metadata)
{
    m_dsoScale = metadata.scale;

    _setupDSOOscilloscope(metadata);

    ui->dsoerrorLed->activate(metadata.status == DS_Error);
    ui->dsosamplingLed->activate(metadata.status == DS_Sampling);

    ui->dsosamplingrateLabel->setText(QString("%1").arg(metadata.samplingRate));
    ui->dsosamplesLabel->setText(QString("%1").arg(metadata.samples));
    ui->dsowindowLabel->setText(QString("%1").arg(metadata.window));

    switch (metadata.mode)
    {
        case DOM_Idle:
            ui->dsomodeLabel->setText("Idle");
            break;
        case DOM_VDC:
            ui->dsomodeLabel->setText("VDC");
            break;
        case DOM_VAC:
            ui->dsomodeLabel->setText("VAC");
            break;
        case DOM_ADC:
            ui->dsomodeLabel->setText("ADC");
            break;
        case DOM_AAC:
            ui->dsomodeLabel->setText("AAC");
            break;
    }
}
//=============================================================================
void MainWindow::centralStateChanged(blew::CentralState newState)
{
    if (newState == blew::CS_On) ui->scanButton->setEnabled(true);
}
//=============================================================================
void MainWindow::peripheralDiscovered(blew::ble_peripheral peripheral)
{
    if (peripheral->name().empty()) return;  // filter all unnamed devices
    QString uuid(peripheral->uuid().toString().c_str());
    QString name(peripheral->name().c_str());
    gui::UltraEntry e(name);
    e.variant = uuid;
    ui->connectionSelector->addEntry(e);
}
//=============================================================================
void MainWindow::peripheralConnected(blew::ble_peripheral peripheral)
{
    m_peripheral = peripheral;
    ui->connectButton->setText("Disconnect");
    peripheral->discoverServices();
}
//=============================================================================
void MainWindow::peripheralDisconnected(blew::ble_peripheral peripheral)
{
    m_peripheral.reset();
    ui->connectButton->setText("Connect");
}
//=============================================================================
void MainWindow::peripheralUpdatedRSSI(blew::ble_peripheral peripheral) {}
//=============================================================================
void MainWindow::servicesDiscovered(blew::ble_peripheral peripheral)
{
    // discover all the characteristics (one level deep)
    peripheral->discoverCharacteristics();
}
//=============================================================================
void MainWindow::includedServicesDiscovered(blew::ble_service peripheral) {}
//=============================================================================
void MainWindow::charsDiscovered(blew::ble_service service)
{
#if DEBUG_FLAG == true
    fprintf(stderr, "discovered chars for service %s :\n", service->uuid().toString().c_str());
    for (auto&& el : service->getChars())
    {
        fprintf(stderr, "    %s\n", el->uuid().toString().c_str());
    }
    fprintf(stderr, "\n");
#endif

    auto servid = service->uuid();
    blew::ble_char c;

    if (servid == pokit_status_service)
    {
        c = service->getChar(pokit_device_ch);
        if (c) c->readValue();

        c = service->getChar(pokit_status_ch);
        if (c)
        {
            c->subscribe();
            c->readValue();
        }

        c = service->getChar(pokit_button_ch);
        if (c) c->subscribe();

        c = service->getChar(pokit_torch_ch);
        if (c) c->subscribe();
    }
    else if (servid == pokit_multimeter_service)
    {
        c = service->getChar(pokit_multimeter_reading_ch);
        if (c) c->subscribe();
    }
    else if (servid == pokit_dso_service)
    {
        c = service->getChar(pokit_dso_metadata_ch);
        if (c) c->subscribe();

        c = service->getChar(pokit_dso_reading_ch);
        if (c) c->subscribe();
    }
}
//=============================================================================
void MainWindow::charValueUpdated(blew::ble_char characteristic)
{
    auto charuuid    = characteristic->uuid();
    blew::Buffer buf = characteristic->value();

#if DEBUG_FLAG == true
    DEBUG_BUFFER(charuuid, buf);
#endif

    if (charuuid == pokit_device_ch)
    {
        DeviceData data = {};
        buf.copyTo(&data, sizeof(data));

        _updateDevData(data);
    }
    else if (charuuid == pokit_status_ch)
    {
        DeviceStatus status = {};
        buf.copyTo(&status, sizeof(status));

        _updateDevStatus(status);
    }
    else if (charuuid == pokit_multimeter_reading_ch)
    {
        MMReading reading = {};
        buf.copyTo(&reading, sizeof(reading));

        ui->mmrangeLabel->setText(_mmrangeToStr(reading.range, reading.mode));
        ui->mmmodeLabel->setText(_mmmodeToStr(reading.mode));
        _updateMMLeds(reading.mode, reading.status);
        ui->mmvalue->setValue(reading.value);

        m_mmrxTimer.start();
        ui->mmrxled->activate(true);
    }
    else if (charuuid == pokit_button_ch)
    {
        DeviceButton b = {};
        buf.copyTo(&b, sizeof(b));
    }
    else if (charuuid == pokit_torch_ch)
    {
        uint8_t byte = 0;
        buf.copyTo(&byte, 1);
        ui->torchButton->setState(byte);
    }
    else if (charuuid == pokit_dso_metadata_ch)
    {
        DSOMetadata d = {};
        buf.copyTo(&d, sizeof(d));
        _dsoMetadata(d);
    }
    else if (charuuid == pokit_dso_reading_ch)
    {
        DSOReading r = {};
        buf.copyTo(&r, sizeof(r));
        _dsoReading(r, buf.size / 2);

        if (sizeof(r) < buf.size)
            PRINT("DSO reading size mismatch, received %u, expected %lu", buf.size, sizeof(r));
    }
}
//=============================================================================
void MainWindow::charValueWritten(blew::ble_char characteristic)
{
    if (characteristic->uuid() == pokit_torch_ch)
    {
        auto c = characteristic->value();

        bool v = false;
        c.copyTo(&v, 1);

        ui->torchButton->setState(v);
    }
}
//=============================================================================
void MainWindow::charUpdatedSubscribeStatus(blew::ble_char characteristic, bool ok) {}
//=============================================================================
void MainWindow::_onScanButtonClick()
{
    if (isScanning()) return;

    clearBLEPeripherals();
    ui->connectionSelector->clear();

    startBLEScanning();

    m_scanTimer.setSingleShot(true);
    m_scanTimer.setInterval(30000);  // 30 seconds
    m_scanTimer.start();
}
//=============================================================================
void MainWindow::_onScanTimerTimeout()
{
    stopBLEScanning();
    ui->scanButton->setState(false);
}
//=============================================================================
void MainWindow::_onMMRxTimerTimeout()
{
    ui->mmrxled->activate(false);
    ui->mmmodeLabel->setText(_mmmodeToStr(MM_IDLE));
}
//=============================================================================
void MainWindow::_onDSORxTimerTimeout() { ui->dsotriggerButton->setState(false); }
//=============================================================================
void MainWindow::_deviceSelected(const gui::UltraEntry*)
{
    stopBLEScanning();
    m_scanTimer.stop();
    ui->scanButton->setState(false);
}
//=============================================================================
void MainWindow::_onConnectButtonClick()
{
    if (m_peripheral)  // already connected
        m_peripheral->disconnect();
    else if (ui->connectionSelector->current())
        connectBLEPeripheral(ui->connectionSelector->current()->variant.toString().toStdString());
}
//=============================================================================
void MainWindow::_onDeviceModeChange(int32_t id, void* p)
{
    // noop
}
//=============================================================================
void MainWindow::_onMultimeterModeChange(int32_t id, void* p)
{
    if (id < 0) return;
    _setupMMRangeSelector((MultimeterMode)id);
    _updateDeviceMMMode();
}
//=============================================================================
void MainWindow::_onMultimeterRangeSelectorPress(const gui::UltraEntry* entry) { _updateDeviceMMMode(); }
//=============================================================================
void MainWindow::_onDSOModeChange(int32_t id, void* p) { _updateDeviceDSOMode(); }
//=============================================================================
void MainWindow::_onDSORangeSelectorPress(const gui::UltraEntry*) { _updateDeviceDSOMode(); }
//=============================================================================
void MainWindow::_onDSOMeasureChange(int32_t id, void* p) { _updateDeviceDSOMode(); }
//=============================================================================
void MainWindow::_onTorchButtonChange(bool newstate)
{
    if (!m_peripheral) return;

    blew::ble_char c = m_peripheral->characteristic_r(pokit_torch_ch);
    if (!c) return;

    uint8_t x = newstate ? 1 : 0;
    blew::Buffer b(&x, 1);

    MMSettings mmsettings = {};

    c->writeValue(b);
}
//=============================================================================
void MainWindow::_onDsoTriggerButtonChange(bool state)
{
    if (state)
        _updateDeviceDSOMode();
    else
        _updateDeviceDSOMode(true);
}
//=============================================================================
