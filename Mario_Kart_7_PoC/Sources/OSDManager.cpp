#include "OSDManager.hpp"

namespace CTRPluginFramework
{
    _OSDManager*  _OSDManager::_singleton = nullptr;

    OSDMI&  OSDMI::operator=(const std::string &str)
    {
        OSDManager.Lock();
        std::get<1>(data) = str;
        std::get<4>(data) = true;
        OSDManager.Unlock();
        return (*this);
    }

    OSDMI&  OSDMI::operator=(const OSDMITuple &tuple)
    {
        OSDManager.Lock();
        data = tuple;
        OSDManager.Unlock();
        return (*this);
    }

    OSDMI&  OSDMI::SetPos(u32 posX, u32 posY)
    {
        OSDManager.Lock();
        std::get<2>(data) = posX;
        std::get<3>(data) = posY;
        OSDManager.Unlock();
        return (*this);
    }

    OSDMI&  OSDMI::SetScreen(bool topScreen)
    {
        OSDManager.Lock();
        std::get<0>(data) = topScreen;
        OSDManager.Unlock();
        return (*this);
    }

    OSDMI&  OSDMI::Enable(void)
    {
        OSDManager.Lock();
        std::get<4>(data) = true;
        OSDManager.Unlock();
        return (*this);
    }

    OSDMI&  OSDMI::Disable(void)
    {
        OSDManager.Lock();
        std::get<4>(data) = false;
        OSDManager.Unlock();
        return (*this);
    }

    OSDMI::OSDMI(OSDMITuple &tuple) : data(tuple)
    {
       
    }

    _OSDManager::~_OSDManager(void)
    {
        OSD::Stop(OSDCallback);
        _items.clear();
    }

    _OSDManager* _OSDManager::GetInstance(void)
    {
        if (_singleton == nullptr)
            _singleton = new _OSDManager;
        return (_singleton);
    }

    void    _OSDManager::Lock(void)
    {
        LightLock_Lock(&_lock);
    }

    void    _OSDManager::Unlock(void)
    {
        LightLock_Unlock(&_lock);
    }

    OSDMI   _OSDManager::operator[](const std::string &key)
    {
        Lock();
        OSDMI i(_items[key]);
        Unlock();
        return (i);
    }

    void    _OSDManager::Remove(const std::string& key)
    {
        Lock();
        _items.erase(key);
        Unlock();
    }

    _OSDManager::_OSDManager(void)
    {
        LightLock_Init(&_lock);
        OSD::Run(OSDCallback);
    }

    bool    _OSDManager::OSDCallback(const Screen &screen)
    {
        _OSDManager &manager = OSDManager;
        
        manager.Lock();

        // If there's no item to draw
        if (manager._items.empty())
        {
            manager.Unlock();
            return (false);
        }

        bool    fbEdited = false;

        // Iterate through all our items
        for (auto item : manager._items)
        {
            auto &t = item.second;
            std::string &str = std::get<1>(t);

            // If item is disabled or if the item is empty
            if (!std::get<4>(t) || str.empty())
                continue;

            // If wanted screen correspond to the screen received, draw the item
            if (std::get<0>(t) == screen.IsTop)
            {                
                screen.Draw(str, std::get<2>(t), std::get<3>(t));
                fbEdited = true;
            }
        }

        manager.Unlock();
        return (fbEdited);
    }
}
