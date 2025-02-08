#include <linux/uinput.h>
#include <thread>
#define BUSY -1
#define FREE 0
#define DOWN 1
#define MOVE 2
#define UP 3
struct screen
{
    int width{};
    int height{};
    int orientation{};
    int fd{};
};

struct touchOBJ
{
    int x{0};
    int y{0};
    int id{0};
    int PySlot{-1};//如果数据来自于物理触摸屏，这个成员会被赋值
    int TRACKING_ID{0};
    int STATUS{FREE};
    bool isDown{false};
    bool isUse{false};
    bool isNeedMove{false};
    bool isNeedDown{false};
    bool isNeedUp{false};
    bool IsFirstDown{false};
};

class Vector2
{
public:
    Vector2();
    Vector2(float x, float y);
    Vector2(int x, int y);
    Vector2(Vector2 &va);
    Vector2& operator=(const Vector2& other);
    float x{};
    float y{};
};

class Vector3
{
public:
    Vector3();
    Vector3(int x,int y,int z);
    int x{};
    int y{};
    int z{};
};

class touch
{
public:
    touch();
    ~touch();
    int touch_down(Vector2 pos);//按下,id可以是任何数
    void touch_up(const int& slotId);//释放,id可以是任何数
    void touch_move(const int& slotId,Vector2 pos);//x轴移动到x，y轴移动到y
private:
    uinput_user_dev usetup;//驱动信息
    int uinputFd{};//uinput的文件标识符
    std::thread PTScreenEventToFingerThread{};//将物理触摸屏的Event转化存到Finger数组的线程
    std::thread GetScreenorientationThread{};//循环获取屏幕方向的线程
    float screenToTouchRatio{};//比例
    std::vector<input_event> VirtualFingersEvent=std::vector<input_event>();
    std::vector<input_event> FingersEvent=std::vector<input_event>();
    bool isWriting{false};//是否正在写入
    bool FreeSlot[10]={false,false,false,false,false,false,false,false,false,false};
    int currentSlot{-1};
    int PhysicalSlot_2_FingerSlot[10]={};
    int FingerSlot_2_TRACKING_ID[10]={16,17,18,19,20,21,22,23,24,25};
    input_event event_BTN_TOUCH_UP=input_event{.type=EV_KEY,.code=BTN_TOUCH,.value=UP};
    input_event event_BTN_TOUCH_DOWN=input_event{.type=EV_KEY,.code=BTN_TOUCH,.value=DOWN};
    input_event event_BTN_TOOL_FINGER_UP=input_event{.type=EV_KEY,.code=BTN_TOOL_FINGER,.value=UP};
    input_event event_BTN_TOOL_FINGER_DOWN=input_event{.type=EV_KEY,.code=BTN_TOOL_FINGER,.value=DOWN};
    int currentTrakingID{0};
    screen screenInfo = {};//屏幕信息
    screen touchScreenInfo = {};//触摸屏信息
private:
    int GetPyFinger(int slot);
    bool IsFirst();
    int GetTRACKING_ID(int slot);
    std::vector<input_event> createNewFinger(int x, int y, int &retSlot);
    std::vector<input_event> createNewFinger(int x, int y);
    int GetNoUseIndex(); // 获取一个没有使用过的finger
    bool IsLast();
    void GetScrorientation(); // 循环获取屏幕方向
    std::string exec(std::string command);
    Vector2 rotatePointx(Vector2 pos, const Vector2 wh);
    // 遍历Finger数组并发送
    void upLoadV2();//原始数据直接发送
    void PTScreenEventToFinger();
    void input_ABS_MT_TRACKING_ID(int id);
    void input_ABS_MT_SLOT(int NewSlot);
    int GetNewSlotByPy(int Pyslot);
    // 将物理触摸屏的Event转化存到Finger数组
    void emit(int fd,input_event ie);//将ie写入fd
    void InitTouchScreenInfo();//初始化物理触摸屏info
    void InitScreenInfo();//初始化屏幕Info
};