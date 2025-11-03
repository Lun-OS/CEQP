#pragma once

// 字体重载函数
void ReloadFonts();

// 设置保存/加载函数
void SaveSettings();
void LoadSettings();

// 应用禁止截图属性（DWM/DisplayAffinity）
void ApplyScreenshotExclusion(bool enable);
