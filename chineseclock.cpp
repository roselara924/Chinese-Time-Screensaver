#define NOMINMAX

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// ============================================================
// 全局变量
// ============================================================
ULONG_PTR g_gdiplusToken = 0;
POINT g_startMousePos = { 0, 0 };
bool g_mouseReady = false;

// ============================================================
// 常量定义
// ============================================================
const double PI = 3.14159265358979323846;

// ============================================================
// 工具函数：角度转弧度
// ============================================================
double DegToRad(double deg)
{
    return deg * PI / 180.0;
}

// ============================================================
// 工具函数：根据角度和半径计算圆周坐标
// 角度规则：0 度在正上方，顺时针旋转
// ============================================================
PointF GetCirclePoint(float cx, float cy, float radius, double angleDeg)
{
    double rad = DegToRad(angleDeg - 90.0);

    float x = cx + static_cast<float>(cos(rad) * radius);
    float y = cy + static_cast<float>(sin(rad) * radius);

    return PointF(x, y);
}

// ============================================================
// 工具函数：将中式角度转换为 GDI+ 角度
// 中式角度：0 度在正上方，顺时针
// GDI+ 角度：0 度在正右方，顺时针
// ============================================================
float ToGdiAngle(double chineseAngleDeg)
{
    return static_cast<float>(chineseAngleDeg - 90.0);
}

// ============================================================
// 工具函数：绘制以半径定义的圆
// ============================================================
void DrawEllipseByRadius(Graphics& g, float cx, float cy, float radius, Pen& pen)
{
    g.DrawEllipse(
        &pen,
        cx - radius,
        cy - radius,
        radius * 2.0f,
        radius * 2.0f
    );
}

// ============================================================
// 工具函数：填充圆
// ============================================================
void FillEllipseByRadius(Graphics& g, float cx, float cy, float radius, Brush& brush)
{
    g.FillEllipse(
        &brush,
        cx - radius,
        cy - radius,
        radius * 2.0f,
        radius * 2.0f
    );
}

// ============================================================
// 工具函数：绘制居中文字
// ============================================================
void DrawCenterText(
    Graphics& g,
    const std::wstring& text,
    Font& font,
    Brush& brush,
    float x,
    float y,
    float rectW = 130.0f,
    float rectH = 44.0f)
{
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    RectF rect(
        x - rectW / 2.0f,
        y - rectH / 2.0f,
        rectW,
        rectH
    );

    g.DrawString(
        text.c_str(),
        -1,
        &font,
        rect,
        &fmt,
        &brush
    );
}

// ============================================================
// 工具函数：绘制带轻微辉光的居中文字
// ============================================================
void DrawGlowCenterText(
    Graphics& g,
    const std::wstring& text,
    Font& font,
    Color glowColor,
    Color textColor,
    float x,
    float y,
    float rectW = 130.0f,
    float rectH = 44.0f)
{
    SolidBrush glowBrush(glowColor);
    SolidBrush textBrush(textColor);

    DrawCenterText(g, text, font, glowBrush, x - 1.0f, y, rectW, rectH);
    DrawCenterText(g, text, font, glowBrush, x + 1.0f, y, rectW, rectH);
    DrawCenterText(g, text, font, glowBrush, x, y - 1.0f, rectW, rectH);
    DrawCenterText(g, text, font, glowBrush, x, y + 1.0f, rectW, rectH);
    DrawCenterText(g, text, font, textBrush, x, y, rectW, rectH);
}

// ============================================================
// 工具函数：绘制环形扇区
// 用于外圈十二时辰、中圈八刻、内圈十五分的分区填充
//
// 重要：本函数现在按“扇区起点”绘制。
// startAngleDeg 表示该扇区的起点角度，而不是扇区中心角度。
// 角度规则仍然是：0 度在正上方，顺时针旋转。
// ============================================================
void DrawDonutSector(
    Graphics& g,
    float cx,
    float cy,
    float innerRadius,
    float outerRadius,
    double startAngleDeg,
    double sweepAngleDeg,
    double gapAngleDeg,
    Brush& brush)
{
    double realSweep = std::max(0.0, sweepAngleDeg - gapAngleDeg);

    // gapAngleDeg 平均留在扇区两侧，避免相邻扇区粘在一起。
    double startAngle = startAngleDeg + gapAngleDeg / 2.0;
    double endAngle = startAngle + realSweep;

    RectF outerRect(
        cx - outerRadius,
        cy - outerRadius,
        outerRadius * 2.0f,
        outerRadius * 2.0f
    );

    RectF innerRect(
        cx - innerRadius,
        cy - innerRadius,
        innerRadius * 2.0f,
        innerRadius * 2.0f
    );

    GraphicsPath path;

    path.AddArc(
        outerRect,
        ToGdiAngle(startAngle),
        static_cast<float>(realSweep)
    );

    path.AddArc(
        innerRect,
        ToGdiAngle(endAngle),
        static_cast<float>(-realSweep)
    );

    path.CloseFigure();
    g.FillPath(&brush, &path);
}

// ============================================================
// 工具函数：绘制径向刻度线
// ============================================================
void DrawRadialTicks(
    Graphics& g,
    float cx,
    float cy,
    float innerRadius,
    float outerRadius,
    int count,
    int majorEvery,
    Pen& minorPen,
    Pen& majorPen)
{
    for (int i = 0; i < count; ++i)
    {
        bool major = (majorEvery > 0 && i % majorEvery == 0);
        float tickInner = major ? innerRadius : (innerRadius + (outerRadius - innerRadius) * 0.42f);

        double angle = 360.0 * i / count;
        PointF p1 = GetCirclePoint(cx, cy, tickInner, angle);
        PointF p2 = GetCirclePoint(cx, cy, outerRadius, angle);

        g.DrawLine(major ? &majorPen : &minorPen, p1, p2);
    }
}

// ============================================================
// 工具函数：绘制点状刻度
// ============================================================
void DrawDotTicks(
    Graphics& g,
    float cx,
    float cy,
    float radius,
    int count,
    float dotRadius,
    Brush& brush)
{
    for (int i = 0; i < count; ++i)
    {
        double angle = 360.0 * i / count;
        PointF p = GetCirclePoint(cx, cy, radius, angle);

        g.FillEllipse(
            &brush,
            p.X - dotRadius,
            p.Y - dotRadius,
            dotRadius * 2.0f,
            dotRadius * 2.0f
        );
    }
}

// ============================================================
// 工具函数：绘制厚重玉圭 / 短剑式时辰针
// ============================================================
void DrawHourHand(
    Graphics& g,
    float cx,
    float cy,
    float length,
    double angleDeg)
{
    double rad = DegToRad(angleDeg - 90.0);

    float dx = static_cast<float>(cos(rad));
    float dy = static_cast<float>(sin(rad));

    float px = -dy;
    float py = dx;

    float bladeHalfWidth = 9.0f;
    float baseHalfWidth = 15.0f;
    float shoulderDistance = length * 0.28f;
    float tailDistance = 24.0f;

    PointF tip(
        cx + dx * length,
        cy + dy * length
    );

    PointF leftShoulder(
        cx + dx * shoulderDistance + px * bladeHalfWidth,
        cy + dy * shoulderDistance + py * bladeHalfWidth
    );

    PointF leftBase(
        cx - dx * tailDistance + px * baseHalfWidth,
        cy - dy * tailDistance + py * baseHalfWidth
    );

    PointF tail(
        cx - dx * (tailDistance + 16.0f),
        cy - dy * (tailDistance + 16.0f)
    );

    PointF rightBase(
        cx - dx * tailDistance - px * baseHalfWidth,
        cy - dy * tailDistance - py * baseHalfWidth
    );

    PointF rightShoulder(
        cx + dx * shoulderDistance - px * bladeHalfWidth,
        cy + dy * shoulderDistance - py * bladeHalfWidth
    );

    std::vector<PointF> points =
    {
        tip,
        leftShoulder,
        leftBase,
        tail,
        rightBase,
        rightShoulder
    };

    SolidBrush shadowBrush(Color(90, 0, 0, 0));
    std::vector<PointF> shadowPoints = points;
    for (auto& p : shadowPoints)
    {
        p.X += 3.0f;
        p.Y += 3.0f;
    }
    g.FillPolygon(&shadowBrush, shadowPoints.data(), static_cast<INT>(shadowPoints.size()));

    SolidBrush bodyBrush(Color(245, 232, 188, 92));
    Pen outlinePen(Color(230, 255, 236, 150), 1.8f);

    g.FillPolygon(&bodyBrush, points.data(), static_cast<INT>(points.size()));
    g.DrawPolygon(&outlinePen, points.data(), static_cast<INT>(points.size()));

    PointF highlight1(
        cx + dx * 18.0f + px * 2.0f,
        cy + dy * 18.0f + py * 2.0f
    );

    PointF highlight2(
        cx + dx * (length - 22.0f) + px * 2.0f,
        cy + dy * (length - 22.0f) + py * 2.0f
    );

    Pen highlightPen(Color(160, 255, 248, 185), 1.2f);
    g.DrawLine(&highlightPen, highlight1, highlight2);
}

// ============================================================
// 工具函数：绘制青铜细箭式刻针
// ============================================================
void DrawKeHand(
    Graphics& g,
    float cx,
    float cy,
    float length,
    double angleDeg)
{
    double rad = DegToRad(angleDeg - 90.0);

    float dx = static_cast<float>(cos(rad));
    float dy = static_cast<float>(sin(rad));

    float px = -dy;
    float py = dx;

    PointF start(
        cx - dx * 18.0f,
        cy - dy * 18.0f
    );

    PointF end(
        cx + dx * length,
        cy + dy * length
    );

    Pen shadowPen(Color(90, 0, 0, 0), 8.0f);
    shadowPen.SetStartCap(LineCapRound);
    shadowPen.SetEndCap(LineCapRound);
    g.DrawLine(&shadowPen, PointF(start.X + 2.5f, start.Y + 2.5f), PointF(end.X + 2.5f, end.Y + 2.5f));

    Pen bodyPen(Color(235, 88, 190, 150), 4.2f);
    bodyPen.SetStartCap(LineCapRound);
    bodyPen.SetEndCap(LineCapRound);
    g.DrawLine(&bodyPen, start, end);

    PointF arrowBase(
        cx + dx * (length - 22.0f),
        cy + dy * (length - 22.0f)
    );

    PointF left(
        arrowBase.X + px * 8.0f,
        arrowBase.Y + py * 8.0f
    );

    PointF right(
        arrowBase.X - px * 8.0f,
        arrowBase.Y - py * 8.0f
    );

    PointF arrow[3] = { end, left, right };

    SolidBrush arrowBrush(Color(245, 105, 215, 172));
    Pen arrowPen(Color(200, 190, 255, 220), 1.0f);

    g.FillPolygon(&arrowBrush, arrow, 3);
    g.DrawPolygon(&arrowPen, arrow, 3);
}

// ============================================================
// 工具函数：绘制朱红细分针，末端带小圆点
// ============================================================
void DrawFenHand(
    Graphics& g,
    float cx,
    float cy,
    float length,
    double angleDeg)
{
    PointF end = GetCirclePoint(cx, cy, length, angleDeg);
    PointF tail = GetCirclePoint(cx, cy, 20.0f, angleDeg + 180.0);

    Pen shadowPen(Color(80, 0, 0, 0), 4.5f);
    shadowPen.SetStartCap(LineCapRound);
    shadowPen.SetEndCap(LineCapRound);
    g.DrawLine(&shadowPen, PointF(tail.X + 2.0f, tail.Y + 2.0f), PointF(end.X + 2.0f, end.Y + 2.0f));

    Pen bodyPen(Color(245, 210, 68, 54), 2.5f);
    bodyPen.SetStartCap(LineCapRound);
    bodyPen.SetEndCap(LineCapRound);
    g.DrawLine(&bodyPen, tail, end);

    SolidBrush dotBrush(Color(255, 255, 130, 105));
    SolidBrush coreBrush(Color(255, 255, 238, 190));

    g.FillEllipse(&dotBrush, end.X - 6.0f, end.Y - 6.0f, 12.0f, 12.0f);
    g.FillEllipse(&coreBrush, end.X - 2.2f, end.Y - 2.2f, 4.4f, 4.4f);
}

// ============================================================
// 绘制暗金 / 青铜仪表盘背景
// ============================================================
void DrawPlateBackground(Graphics& g, int width, int height, float cx, float cy, float outerRadius)
{
    SolidBrush bgBrush(Color(255, 2, 3, 4));
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    float glowRadius = outerRadius * 1.28f;

    GraphicsPath glowPath;
    glowPath.AddEllipse(
        cx - glowRadius,
        cy - glowRadius,
        glowRadius * 2.0f,
        glowRadius * 2.0f
    );

    PathGradientBrush glowBrush(&glowPath);
    glowBrush.SetCenterColor(Color(70, 48, 36, 12));

    Color surroundColors[1] = { Color(0, 0, 0, 0) };
    INT colorCount = 1;
    glowBrush.SetSurroundColors(surroundColors, &colorCount);

    g.FillPath(&glowBrush, &glowPath);

    SolidBrush plateBrush(Color(24, 28, 21, 13));
    FillEllipseByRadius(g, cx, cy, outerRadius * 1.04f, plateBrush);

    Pen plateOuterPen(Color(90, 128, 95, 44), 3.0f);
    Pen plateInnerPen(Color(55, 190, 150, 80), 1.2f);

    DrawEllipseByRadius(g, cx, cy, outerRadius * 1.04f, plateOuterPen);
    DrawEllipseByRadius(g, cx, cy, outerRadius * 0.985f, plateInnerPen);
}

// ============================================================
// 绘制三层分区环
// 外圈：十二时辰，12 个扇区
// 中圈：八刻，8 个青铜刻度分区
// 内圈：十五分，15 个细分区
// ============================================================
void DrawSegmentRings(
    Graphics& g,
    float cx,
    float cy,
    float outerRadius,
    float middleRadius,
    float innerRadius,
    int shichenIndex,
    int keIndex,
    int fenIndex)
{
    // ------------------------------
    // 外圈十二时辰扇区
    // ------------------------------
    float outerBandInner = outerRadius - 96.0f;
    float outerBandOuter = outerRadius - 2.0f;

    SolidBrush outerBrushA(Color(44, 94, 68, 30));
    SolidBrush outerBrushB(Color(34, 64, 50, 25));
    SolidBrush outerHighlightBrush(Color(105, 206, 158, 68));

    for (int i = 0; i < 12; ++i)
    {
        double startAngle = 360.0 * i / 12.0;
        DrawDonutSector(
            g,
            cx,
            cy,
            outerBandInner,
            outerBandOuter,
            startAngle,
            360.0 / 12.0,
            1.8,
            (i % 2 == 0) ? static_cast<Brush&>(outerBrushA) : static_cast<Brush&>(outerBrushB)
        );
    }

    DrawDonutSector(
        g,
        cx,
        cy,
        outerBandInner,
        outerBandOuter,
        360.0 * shichenIndex / 12.0,
        360.0 / 12.0,
        1.2,
        outerHighlightBrush
    );

    // ------------------------------
    // 中圈八刻青铜环
    // ------------------------------
    float middleBandInner = middleRadius - 58.0f;
    float middleBandOuter = middleRadius - 6.0f;

    SolidBrush middleBrushA(Color(36, 40, 92, 74));
    SolidBrush middleBrushB(Color(28, 30, 70, 58));
    SolidBrush middleHighlightBrush(Color(105, 70, 190, 145));

    for (int i = 0; i < 8; ++i)
    {
        double startAngle = 360.0 * i / 8.0;
        DrawDonutSector(
            g,
            cx,
            cy,
            middleBandInner,
            middleBandOuter,
            startAngle,
            360.0 / 8.0,
            2.2,
            (i % 2 == 0) ? static_cast<Brush&>(middleBrushA) : static_cast<Brush&>(middleBrushB)
        );
    }

    DrawDonutSector(
        g,
        cx,
        cy,
        middleBandInner,
        middleBandOuter,
        360.0 * keIndex / 8.0,
        360.0 / 8.0,
        1.4,
        middleHighlightBrush
    );

    // ------------------------------
    // 内圈十五分细分环
    // ------------------------------
    float innerBandInner = innerRadius - 42.0f;
    float innerBandOuter = innerRadius - 4.0f;

    SolidBrush innerBrushA(Color(30, 92, 38, 32));
    SolidBrush innerBrushB(Color(22, 64, 30, 26));
    SolidBrush innerHighlightBrush(Color(115, 190, 62, 48));

    for (int i = 0; i < 15; ++i)
    {
        double startAngle = 360.0 * i / 15.0;
        DrawDonutSector(
            g,
            cx,
            cy,
            innerBandInner,
            innerBandOuter,
            startAngle,
            360.0 / 15.0,
            1.5,
            (i % 2 == 0) ? static_cast<Brush&>(innerBrushA) : static_cast<Brush&>(innerBrushB)
        );
    }

    DrawDonutSector(
        g,
        cx,
        cy,
        innerBandInner,
        innerBandOuter,
        360.0 * fenIndex / 15.0,
        360.0 / 15.0,
        0.8,
        innerHighlightBrush
    );

    // ------------------------------
    // 环形边界与刻度
    // ------------------------------
    Pen outerBorderPen(Color(150, 184, 136, 58), 2.0f);
    Pen outerWeakPen(Color(90, 130, 96, 50), 1.1f);
    Pen middleBorderPen(Color(130, 82, 178, 142), 1.6f);
    Pen innerBorderPen(Color(130, 182, 82, 64), 1.4f);

    DrawEllipseByRadius(g, cx, cy, outerBandOuter, outerBorderPen);
    DrawEllipseByRadius(g, cx, cy, outerBandInner, outerWeakPen);

    DrawEllipseByRadius(g, cx, cy, middleBandOuter, middleBorderPen);
    DrawEllipseByRadius(g, cx, cy, middleBandInner, outerWeakPen);

    DrawEllipseByRadius(g, cx, cy, innerBandOuter, innerBorderPen);
    DrawEllipseByRadius(g, cx, cy, innerBandInner, outerWeakPen);

    // 所有径向刻度统一改成浅风格。
    // 原来的主刻度较粗，正好会压在“子、丑、寅”和“初刻、二刻”等文字下面，影响识别。
    Pen minorOuterTick(Color(70, 145, 112, 38), 0.8f);
    Pen majorOuterTick(Color(78, 155, 120, 42), 0.9f);
    DrawRadialTicks(g, cx, cy, outerBandInner, outerBandOuter, 60, 5, minorOuterTick, majorOuterTick);

    Pen minorMiddleTick(Color(68, 105, 150, 70), 0.8f);
    Pen majorMiddleTick(Color(76, 116, 165, 78), 0.9f);
    DrawRadialTicks(g, cx, cy, middleBandInner, middleBandOuter, 40, 5, minorMiddleTick, majorMiddleTick);

    Pen minorInnerTick(Color(62, 135, 70, 42), 0.7f);
    Pen majorInnerTick(Color(70, 150, 78, 48), 0.8f);
    DrawRadialTicks(g, cx, cy, innerBandInner, innerBandOuter, 60, 4, minorInnerTick, majorInnerTick);

    // 内圈点刻度不能压在“初、二、三……”这些分字上。
    // 点刻度改到内圈外缘，并降低亮度，只作为辅助读数。
    SolidBrush dotBrush(Color(65, 180, 130, 48));
    DrawDotTicks(g, cx, cy, innerRadius - 7.0f, 15, 1.4f, dotBrush);

    SolidBrush hotDotBrush(Color(160, 255, 210, 120));
    PointF hotDot = GetCirclePoint(cx, cy, innerRadius - 7.0f, 360.0 * fenIndex / 15.0);
    g.FillEllipse(&hotDotBrush, hotDot.X - 2.8f, hotDot.Y - 2.8f, 5.6f, 5.6f);

    // ------------------------------
    // 暗金校表标记线
    //
    // 正午日晷对表时：
    // 外圈：午正 = 12:00 = 从 23:00 子时起点过了 13 小时 = 195 度
    // 中圈：五刻起点 = 第 5 刻 = 4 / 8 圈 = 180 度
    // 内圈：初分起点 = 0 度
    //
    // 这三条线只是低调提示，不参与普通刻度高亮。
    // ------------------------------
    Pen calibrationPen(Color(150, 190, 145, 55), 2.0f);
    Pen calibrationThinPen(Color(110, 190, 145, 55), 1.2f);

    PointF p1;
    PointF p2;

    p1 = GetCirclePoint(cx, cy, outerBandInner - 2.0f, 195.0);
    p2 = GetCirclePoint(cx, cy, outerBandOuter + 8.0f, 195.0);
    g.DrawLine(&calibrationPen, p1, p2);

    p1 = GetCirclePoint(cx, cy, middleBandInner - 2.0f, 180.0);
    p2 = GetCirclePoint(cx, cy, middleBandOuter + 7.0f, 180.0);
    g.DrawLine(&calibrationPen, p1, p2);

    p1 = GetCirclePoint(cx, cy, innerBandInner - 2.0f, 0.0);
    p2 = GetCirclePoint(cx, cy, innerBandOuter + 7.0f, 0.0);
    g.DrawLine(&calibrationThinPen, p1, p2);

    FontFamily markFontFamily(L"Microsoft YaHei");
    Font markFont(&markFontFamily, 12.0f, FontStyleRegular, UnitPixel);
    SolidBrush markBrush(Color(145, 210, 170, 78));

    PointF markText;

    markText = GetCirclePoint(cx, cy, outerBandOuter + 22.0f, 195.0);
    DrawCenterText(g, L"午正", markFont, markBrush, markText.X, markText.Y, 58.0f, 24.0f);

    markText = GetCirclePoint(cx, cy, middleBandOuter + 20.0f, 180.0);
    DrawCenterText(g, L"五刻", markFont, markBrush, markText.X, markText.Y, 58.0f, 24.0f);

    markText = GetCirclePoint(cx, cy, innerBandOuter + 20.0f, 0.0);
    DrawCenterText(g, L"初分", markFont, markBrush, markText.X, markText.Y, 58.0f, 24.0f);
}

// ============================================================
// 绘制环形文字
// ============================================================
void DrawRingLabels(
    Graphics& g,
    float cx,
    float cy,
    float outerRadius,
    float middleRadius,
    float innerRadius,
    int shichenIndex,
    int keIndex,
    int fenIndex)
{
    FontFamily fontFamily(L"Microsoft YaHei");

    Font outerFont(&fontFamily, 34.0f, FontStyleBold, UnitPixel);
    Font middleFont(&fontFamily, 20.0f, FontStyleRegular, UnitPixel);
    Font innerFont(&fontFamily, 14.0f, FontStyleRegular, UnitPixel);

    std::vector<std::wstring> shichen =
    {
        L"子", L"丑", L"寅", L"卯", L"辰", L"巳",
        L"午", L"未", L"申", L"酉", L"戌", L"亥"
    };

    std::vector<std::wstring> ke =
    {
        L"初刻", L"二刻", L"三刻", L"四刻",
        L"五刻", L"六刻", L"七刻", L"八刻"
    };

    std::vector<std::wstring> fen =
    {
        L"初分", L"二分", L"三分", L"四分", L"五分",
        L"六分", L"七分", L"八分", L"九分", L"十分",
        L"十一", L"十二", L"十三", L"十四", L"十五"
    };

    for (int i = 0; i < 12; ++i)
    {
        double angle = 360.0 * i / 12.0 + 15.0;
        PointF p = GetCirclePoint(cx, cy, outerRadius - 52.0f, angle);

        if (i == shichenIndex)
        {
            DrawGlowCenterText(
                g,
                shichen[i],
                outerFont,
                Color(135, 255, 208, 110),
                Color(255, 255, 226, 150),
                p.X,
                p.Y,
                86.0f,
                56.0f
            );
        }
        else
        {
            DrawGlowCenterText(
                g,
                shichen[i],
                outerFont,
                Color(45, 210, 150, 70),
                Color(210, 205, 168, 95),
                p.X,
                p.Y,
                86.0f,
                56.0f
            );
        }
    }

    for (int i = 0; i < 8; ++i)
    {
        double angle = 360.0 * i / 8.0 + 22.5;
        PointF p = GetCirclePoint(cx, cy, middleRadius - 32.0f, angle);

        if (i == keIndex)
        {
            DrawGlowCenterText(
                g,
                ke[i],
                middleFont,
                Color(120, 130, 255, 210),
                Color(245, 190, 255, 220),
                p.X,
                p.Y,
                100.0f,
                36.0f
            );
        }
        else
        {
            DrawGlowCenterText(
                g,
                ke[i],
                middleFont,
                Color(35, 90, 160, 130),
                Color(172, 130, 210, 180),
                p.X,
                p.Y,
                100.0f,
                36.0f
            );
        }
    }

    for (int i = 0; i < 15; ++i)
    {
        double angle = 360.0 * i / 15.0 + 12.0;
        PointF p = GetCirclePoint(cx, cy, innerRadius - 23.0f, angle);

        if (i == fenIndex)
        {
            DrawGlowCenterText(
                g,
                fen[i],
                innerFont,
                Color(120, 255, 140, 90),
                Color(250, 230, 156, 126),
                p.X,
                p.Y,
                70.0f,
                28.0f
            );
        }
        else
        {
            DrawGlowCenterText(
                g,
                fen[i],
                innerFont,
                Color(30, 110, 120, 65),
                Color(150, 170, 120, 100),
                p.X,
                p.Y,
                70.0f,
                28.0f
            );
        }
    }
}

// ============================================================
// 绘制中心装饰
// ============================================================
void DrawCenterDecoration(Graphics& g, float cx, float cy)
{
    SolidBrush outerBrush(Color(100, 118, 82, 42));
    SolidBrush innerBrush(Color(230, 232, 188, 92));
    SolidBrush coreBrush(Color(255, 255, 236, 170));

    Pen outerPen(Color(180, 205, 160, 85), 2.2f);
    Pen innerPen(Color(220, 255, 232, 150), 1.4f);

    FillEllipseByRadius(g, cx, cy, 24.0f, outerBrush);
    DrawEllipseByRadius(g, cx, cy, 24.0f, outerPen);

    FillEllipseByRadius(g, cx, cy, 13.0f, innerBrush);
    DrawEllipseByRadius(g, cx, cy, 13.0f, innerPen);

    FillEllipseByRadius(g, cx, cy, 5.2f, coreBrush);
}

// ============================================================
// 绘制底部时间读数
// ============================================================
void DrawBottomReadout(
    Graphics& g,
    int width,
    int height,
    float cx,
    float cy,
    float outerRadius,
    int hour,
    int minute,
    int second,
    int shichenIndex,
    int keIndex,
    int fenIndex)
{
    FontFamily fontFamily(L"Microsoft YaHei");
    Font titleFont(&fontFamily, 26.0f, FontStyleBold, UnitPixel);
    Font smallFont(&fontFamily, 15.0f, FontStyleRegular, UnitPixel);

    const wchar_t* shichenName[12] =
    {
        L"子", L"丑", L"寅", L"卯", L"辰", L"巳",
        L"午", L"未", L"申", L"酉", L"戌", L"亥"
    };

    const wchar_t* keName[8] =
    {
        L"初刻", L"二刻", L"三刻", L"四刻",
        L"五刻", L"六刻", L"七刻", L"八刻"
    };

    const wchar_t* fenName[15] =
    {
        L"初分", L"二分", L"三分", L"四分", L"五分",
        L"六分", L"七分", L"八分", L"九分", L"十分",
        L"十一分", L"十二分", L"十三分", L"十四分", L"十五分"
    };

    wchar_t normalTimeText[128];
    swprintf_s(
        normalTimeText,
        L"现行时间  %02d:%02d:%02d",
        hour,
        minute,
        second
    );

    wchar_t chineseTimeText[128];
    swprintf_s(
        chineseTimeText,
        L"中式时刻  %s时 %s %s",
        shichenName[shichenIndex],
        keName[keIndex],
        fenName[fenIndex]
    );

    float panelY = cy + outerRadius + 28.0f;
    if (panelY + 62.0f > height)
    {
        panelY = static_cast<float>(height) - 68.0f;
    }

    RectF panelRect(
        cx - outerRadius,
        panelY,
        outerRadius * 2.0f,
        54.0f
    );

    GraphicsPath panelPath;
    panelPath.AddRectangle(panelRect);

    SolidBrush panelBrush(Color(45, 18, 15, 10));
    Pen panelPen(Color(95, 150, 112, 58), 1.2f);

    g.FillRectangle(&panelBrush, panelRect);
    g.DrawRectangle(&panelPen, panelRect.X, panelRect.Y, panelRect.Width, panelRect.Height);

    StringFormat leftFmt;
    leftFmt.SetAlignment(StringAlignmentNear);
    leftFmt.SetLineAlignment(StringAlignmentCenter);

    StringFormat rightFmt;
    rightFmt.SetAlignment(StringAlignmentFar);
    rightFmt.SetLineAlignment(StringAlignmentCenter);

    SolidBrush mainTextBrush(Color(235, 230, 196, 130));
    SolidBrush subTextBrush(Color(160, 130, 170, 128));

    RectF leftTimeRect(
        panelRect.X + 24.0f,
        panelRect.Y,
        panelRect.Width / 2.0f - 30.0f,
        panelRect.Height
    );

    RectF rightTimeRect(
        cx,
        panelRect.Y,
        panelRect.Width / 2.0f - 24.0f,
        panelRect.Height
    );

    g.DrawString(normalTimeText, -1, &titleFont, leftTimeRect, &leftFmt, &mainTextBrush);
    g.DrawString(chineseTimeText, -1, &titleFont, rightTimeRect, &rightFmt, &mainTextBrush);
}

// ============================================================
// 绘制中式钟表主体
// ============================================================
void DrawChineseClock(Graphics& g, int width, int height)
{
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.SetCompositingQuality(CompositingQualityHighQuality);

    float cx = width / 2.0f;
    float cy = height / 2.0f - 12.0f;

    float baseRadius = std::min(width, height) * 0.42f;

    float outerRadius = baseRadius;
    float middleRadius = baseRadius * 0.72f;
    float innerRadius = baseRadius * 0.43f;

    // ========================================================
    // 当前时间计算
    //
    // 子时从 23:00 开始：
    // 23:00 - 00:59 为子时
    // 01:00 - 02:59 为丑时
    // 一个时辰 = 120 分钟
    // 一个时辰 = 8 刻
    // 一刻 = 15 分钟
    // ========================================================
    std::time_t now = std::time(nullptr);
    std::tm localTime{};

#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    int hour = localTime.tm_hour;
    int minute = localTime.tm_min;
    int second = localTime.tm_sec;

    double totalMinutes =
        hour * 60.0 +
        minute +
        second / 60.0;

    double chineseMinutes = fmod(totalMinutes + 60.0, 1440.0);
    double minuteInShichen = fmod(chineseMinutes, 120.0);
    double minuteInKe = fmod(minuteInShichen, 15.0);

    int shichenIndex = static_cast<int>(chineseMinutes / 120.0) % 12;
    int keIndex = static_cast<int>(minuteInShichen / 15.0) % 8;
    int fenIndex = static_cast<int>(minuteInKe) % 15;

    if (fenIndex < 0)
    {
        fenIndex = 0;
    }

    if (fenIndex > 14)
    {
        fenIndex = 14;
    }

    // 扇区起点制：
    // 子时从 0 度开始，不再按西式钟表那样把文字所在位置当作整点中心。
    // 时辰、刻、分三根指针都从当前扇区起点开始推进。
    // 因此这里不再回退半个分区。
    // 例：12:00:00 正午对表时：
    // shiAngle = 195 度，keAngle = 180 度，fenAngle = 0 度，显示“午时 五刻 初分”。
    double shiAngle = chineseMinutes / 1440.0 * 360.0;
    double keAngle = minuteInShichen / 120.0 * 360.0;
    double fenAngle = minuteInKe / 15.0 * 360.0;

    // ========================================================
    // 背景与三层仪表盘分区
    // ========================================================
    DrawPlateBackground(g, width, height, cx, cy, outerRadius);

    DrawSegmentRings(
        g,
        cx,
        cy,
        outerRadius,
        middleRadius,
        innerRadius,
        shichenIndex,
        keIndex,
        fenIndex
    );

    DrawRingLabels(
        g,
        cx,
        cy,
        outerRadius,
        middleRadius,
        innerRadius,
        shichenIndex,
        keIndex,
        fenIndex
    );

    // ========================================================
    // 三根指针
    // 时辰针：最长、厚重、暗金玉圭 / 短剑式
    // 刻针：中长、青铜青绿色细箭式
    // 分针：最短、朱红细针，末端小圆点
    // ========================================================
    DrawHourHand(g, cx, cy, outerRadius - 92.0f, shiAngle);
    DrawKeHand(g, cx, cy, middleRadius - 48.0f, keAngle);
    DrawFenHand(g, cx, cy, innerRadius - 38.0f, fenAngle);

    DrawCenterDecoration(g, cx, cy);

    // ========================================================
    // 底部读数
    // ========================================================
    DrawBottomReadout(
        g,
        width,
        height,
        cx,
        cy,
        outerRadius,
        hour,
        minute,
        second,
        shichenIndex,
        keIndex,
        fenIndex
    );
}

// ============================================================
// 窗口过程
// ============================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        SetTimer(hwnd, 1, 33, nullptr);

        GetCursorPos(&g_startMousePos);
        g_mouseReady = true;

        return 0;
    }

    case WM_TIMER:
    {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;

        // 双缓冲，减少屏幕闪烁
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, memBitmap));

        Graphics g(memDC);
        DrawChineseClock(g, width, height);

        BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (g_mouseReady)
        {
            POINT p;
            GetCursorPos(&p);

            int dx = abs(p.x - g_startMousePos.x);
            int dy = abs(p.y - g_startMousePos.y);

            if (dx > 5 || dy > 5)
            {
                PostQuitMessage(0);
            }
        }

        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        PostQuitMessage(0);
        return 0;
    }

    case WM_DESTROY:
    {
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// ============================================================
// 工具函数：转换为小写字符串
// 用于判断屏保启动参数 /s /c /p
// ============================================================
std::wstring ToLowerString(const std::wstring& input)
{
    std::wstring output = input;

    std::transform(
        output.begin(),
        output.end(),
        output.begin(),
        [](wchar_t ch)
        {
            return static_cast<wchar_t>(towlower(ch));
        }
    );

    return output;
}

// ============================================================
// 程序入口
//
// Windows 屏保常见参数：
// /s        正式启动屏保
// /c        配置屏保
// /p hwnd   屏保设置窗口里的小预览
//
// 当前版本：
// /s 或无参数：全屏运行
// /c：提示暂无设置
// /p：暂不处理预览，直接退出
// ============================================================
int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR lpCmdLine,
    int)
{
    std::wstring cmdLine = lpCmdLine ? lpCmdLine : L"";
    std::wstring lowerCmd = ToLowerString(cmdLine);

    // ========================================================
    // 配置窗口
    // 右键 .scr 选择“配置”时会进入这里
    // ========================================================
    if (lowerCmd.find(L"/c") != std::wstring::npos ||
        lowerCmd.find(L"-c") != std::wstring::npos)
    {
        MessageBoxW(
            nullptr,
            L"中式钟表屏保暂无配置项。",
            L"中式钟表屏保",
            MB_OK | MB_ICONINFORMATION
        );

        return 0;
    }

    // ========================================================
    // 小预览窗口
    // Windows 屏保设置界面里的小框预览会进入这里
    // 当前先不实现，避免小窗口里乱画
    // ========================================================
    if (lowerCmd.find(L"/p") != std::wstring::npos ||
        lowerCmd.find(L"-p") != std::wstring::npos)
    {
        return 0;
    }

    // ========================================================
    // 初始化 GDI+
    // ========================================================
    GdiplusStartupInput gdiplusStartupInput;

    if (GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr) != Ok)
    {
        MessageBoxW(nullptr, L"GDI+ 初始化失败", L"Error", MB_ICONERROR);
        return -1;
    }

    // ========================================================
    // 注册窗口类
    // ========================================================
    const wchar_t CLASS_NAME[] = L"ChineseClockSaverWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    RegisterClassW(&wc);

    // ========================================================
    // 获取屏幕尺寸
    // ========================================================
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // ========================================================
    // 创建全屏无边框窗口
    // ========================================================
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        CLASS_NAME,
        L"中式钟表屏保",
        WS_POPUP,
        0,
        0,
        screenW,
        screenH,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd)
    {
        GdiplusShutdown(g_gdiplusToken);
        return -1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // 隐藏鼠标
    ShowCursor(FALSE);

    // ========================================================
    // 消息循环
    // ========================================================
    MSG msg{};

    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 恢复鼠标
    ShowCursor(TRUE);

    // ========================================================
    // 关闭 GDI+
    // ========================================================
    GdiplusShutdown(g_gdiplusToken);

    return 0;
}
