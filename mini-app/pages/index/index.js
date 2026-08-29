Page({
  scale: 1.0,
  translateX: 0,
  translateY: 0,
  lastDistance: 0,
  isTwoFinger: false,
  dragOffsetX: 0,
  dragOffsetY: 0,
  canvas: null,
  ctx: null,
  dpr: 1,
  canvasW: 0,
  canvasH: 0,
  // 节流标记，减少频繁重绘缓解闪烁
  isDrawing: false,

  //==== 新增导航相关变量 ====
  showRoute: false,
  // 导航路径点：起点 → 途径点 →终点(1号车位)，你后续可按需修改坐标
  routePoints: [
    { x: 5, y: 175 },
    { x: 270, y: 175 },
    { x: 320, y: 340 },
    { x: 480, y: 340 }
  ],

  onReady() {
    const query = wx.createSelectorQuery()
    query.select('#parkCanvas')
      .fields({ node: true, size: true })
      .exec((res) => {
        this.canvas = res[0].node
        this.ctx = this.canvas.getContext('2d')
        // ✅替换废弃接口 wx.getSystemInfoSync
        const windowInfo = wx.getWindowInfo()
        this.dpr = windowInfo.pixelRatio
        this.canvas.width = res[0].width * this.dpr
        this.canvas.height = res[0].height * this.dpr
        this.canvasW = res[0].width
        this.canvasH = res[0].height
        this.drawMap()
      })
  },

  drawMap() {
    if (!this.ctx) return
    const ctx = this.ctx
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height)
    ctx.save()
    ctx.scale(this.dpr, this.dpr)
    ctx.translate(this.translateX, this.translateY)
    ctx.scale(this.scale, this.scale)

    const w = this.canvasW
    const h = this.canvasH
    const cellW = w * 0.16
    const cellH = h * 0.16
    const roadGap = w * 0.14
    const leftMargin = w * 0.08

    //=========== 绘制中间横向道路3 ===========
    const topRowBottomY = h * 0.08 + cellH;
    const roadCenterY1 = topRowBottomY + roadGap / 2;
    const roadLineWidth = 20;
    ctx.beginPath();
    ctx.strokeStyle = "#aaaaaa";
    ctx.lineWidth = roadLineWidth;
    ctx.moveTo(leftMargin - 2 * cellW - 2 * roadGap, roadCenterY1);
    ctx.lineTo(leftMargin + cellW * 8 + roadGap * 3, roadCenterY1);
    ctx.stroke();

    //=========== 绘制第横向道路1 ===========
    const roadCenterY2 = roadCenterY1 - 2 * cellH - 2 * roadGap;
    ctx.beginPath();
    ctx.moveTo(leftMargin - 2 * cellW - 2 * roadGap, roadCenterY2);
    ctx.lineTo(leftMargin + cellW * 5 + roadGap * 2, roadCenterY2);
    ctx.stroke();

    //=========== 绘制第横向道路2 ===========
    const roadCenterY3 = roadCenterY1 - cellH - roadGap;
    ctx.beginPath();
    ctx.moveTo(leftMargin - 2 * cellW - 2 * roadGap, roadCenterY3);
    ctx.lineTo(leftMargin + cellW * 8 + roadGap * 3, roadCenterY3);
    ctx.stroke();

    //=========== 绘制第横向道路4 ===========
    const roadCenterY5 = roadCenterY1 + cellH + roadGap;
    ctx.beginPath();
    ctx.moveTo(leftMargin - 2 * cellW - 2 * roadGap, roadCenterY5);
    ctx.lineTo(leftMargin + cellW * 8 + roadGap * 3, roadCenterY5);
    ctx.stroke();

    //=========== 绘制第横向道路5 ===========
    const roadCenterY6 = roadCenterY1 + 3 * cellH + 4 * roadGap;
    ctx.beginPath();
    ctx.moveTo(leftMargin - 2 * cellW - 0.5 * roadGap, roadCenterY6);
    ctx.lineTo(leftMargin + 5 * cellW + 2 * roadGap, roadCenterY6);
    ctx.stroke();

    //=========== 绘制第竖直向道路2 ===========
    const roadCenterY4 = roadCenterY1 - 2 * cellH - 2 * roadGap;
    ctx.beginPath();
    ctx.moveTo(leftMargin - 0.5 * roadGap, roadCenterY4);
    ctx.lineTo(leftMargin - 0.5 * roadGap, roadCenterY1 + cellH + roadGap);
    ctx.stroke();

    //=========== 绘制第竖直道路1 ===========
    ctx.beginPath();
    ctx.moveTo(leftMargin - 1.5 * roadGap - 2 * cellW, roadCenterY4);
    ctx.lineTo(leftMargin - 1.5 * roadGap - 2 * cellW, roadCenterY1 + cellH + roadGap);
    ctx.stroke();

    //=========== 绘制第竖直道路3 ===========
    ctx.beginPath();
    ctx.moveTo(leftMargin + 0.5 * roadGap + 2 * cellW, roadCenterY4);
    ctx.lineTo(leftMargin + 0.5 * roadGap + 2 * cellW, roadCenterY1 + cellH + 4 * roadGap + 2 * cellH);
    ctx.stroke();

    //=========== 绘制第竖直道路4 ===========
    ctx.beginPath();
    ctx.moveTo(leftMargin + 1.5 * roadGap + 5 * cellW, roadCenterY4);
    ctx.lineTo(leftMargin + 1.5 * roadGap + 5 * cellW, roadCenterY1 + cellH + 4 * roadGap + 2 * cellH);
    ctx.stroke();

    //=========== 绘制第竖直道路5 ===========
    ctx.beginPath();
    ctx.moveTo(leftMargin + 2.5 * roadGap + 8 * cellW, roadCenterY3);
    ctx.lineTo(leftMargin + 2.5 * roadGap + 8 * cellW, roadCenterY1 + cellH + roadGap);
    ctx.stroke();

    //=========== 绘制第竖直道路6 ===========
    const roadCenterY7 = roadCenterY1 + cellH + roadGap;
    ctx.beginPath();
    ctx.moveTo(leftMargin - 2 * cellW - 0.5 * roadGap, roadCenterY7);
    ctx.lineTo(leftMargin - 2 * cellW - 0.5 * roadGap, roadCenterY1 + 3 * cellH + 4 * roadGap + 10);
    ctx.stroke();

    //=========== 绘制车位 ===========
    this.drawParking(ctx, leftMargin, h * 0.08, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + cellW, h * 0.08, cellW, cellH, "8")
    this.drawParking(ctx, leftMargin + cellW * 2 + roadGap, h * 0.08, cellW, cellH, "7")
    this.drawParking(ctx, leftMargin + cellW * 3 + roadGap, h * 0.08, cellW, cellH, "6")
    this.drawParking(ctx, leftMargin + cellW * 4 + roadGap, h * 0.08, cellW, cellH, "5")
    this.drawParking(ctx, -cellW + leftMargin - roadGap, h * 0.08, cellW, cellH, "")
    this.drawParking(ctx, -cellW * 2 + leftMargin - roadGap, h * 0.08, cellW, cellH, "")
    let roadY = h * 0.08 + cellH + roadGap
    //下排车位
    this.drawParking(ctx, leftMargin, roadY, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + cellW, roadY, cellW, cellH, "4")
    this.drawParking(ctx, leftMargin + cellW * 2 + roadGap, roadY, cellW, cellH, "3")
    this.drawParking(ctx, leftMargin + cellW * 3 + roadGap, roadY, cellW, cellH, "2")
    this.drawParking(ctx, leftMargin + cellW * 4 + roadGap, roadY, cellW, cellH, "1")
    this.drawParking(ctx, -cellW + leftMargin - roadGap, roadY, cellW, cellH, "")
    this.drawParking(ctx, -2 * cellW + leftMargin - roadGap, roadY, cellW, cellH, "")
    //多余车位
    this.drawParking(ctx, leftMargin + cellW * 6 + roadGap, h * 0.08, cellW, cellH, "A1")
    this.drawParking(ctx, leftMargin + cellW * 7 + roadGap, h * 0.08, cellW, cellH, "A2")
    this.drawParking(ctx, leftMargin + cellW * 8 + roadGap, h * 0.08, cellW, cellH, "A3")
    this.drawParking(ctx, leftMargin + cellW * 6 + roadGap, roadY, cellW, cellH, "A4")
    this.drawParking(ctx, leftMargin + cellW * 7 + roadGap, roadY, cellW, cellH, "A5")
    this.drawParking(ctx, leftMargin + cellW * 8 + roadGap, roadY, cellW, cellH, "A6")
    this.drawParking(ctx, -cellW + leftMargin - roadGap, -h * 0.08 - roadGap, cellW, cellH, "")
    this.drawParking(ctx, -2 * cellW + leftMargin - roadGap, -h * 0.08 - roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin, -h * 0.08 - roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + cellW, -h * 0.08 - roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + cellW * 2 + roadGap, -h * 0.08 - roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + cellW * 3 + roadGap, -h * 0.08 - roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + cellW * 4 + roadGap, -h * 0.08 - roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin, roadY + cellH + 3 * roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + cellW, roadY + cellH + 3 * roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin - cellW, roadY + cellH + 3 * roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin - 2 * cellW, roadY + cellH + 3 * roadGap, cellW, cellH, "")

    this.drawParking(ctx, leftMargin, roadY + cellH + 3 * roadGap + cellH, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + cellW, roadY + cellH + 3 * roadGap + cellH, cellW, cellH, "")
    this.drawParking(ctx, leftMargin - cellW, roadY + cellH + 3 * roadGap + cellH, cellW, cellH, "")
    this.drawParking(ctx, leftMargin - 2 * cellW, roadY + cellH + 3 * roadGap + cellH, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + 2 * cellW + roadGap, roadY + cellH + 3 * roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + 3 * cellW + roadGap, roadY + cellH + 3 * roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + 4 * cellW + roadGap, roadY + cellH + 3 * roadGap, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + 2 * cellW + roadGap, roadY + cellH + 3 * roadGap + cellH, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + 3 * cellW + roadGap, roadY + cellH + 3 * roadGap + cellH, cellW, cellH, "")
    this.drawParking(ctx, leftMargin + 4 * cellW + roadGap, roadY + cellH + 3 * roadGap + cellH, cellW, cellH, "")

    // ----------绘制导航直线----------
ctx.beginPath();
ctx.moveTo(5, 175);
ctx.lineTo(270, 175);
ctx.strokeStyle = '#1976EC';   //蓝色
ctx.lineWidth = 20;            //线条宽度10
ctx.lineCap = "butt";          //矩形平头线条，无圆角
ctx.stroke();
// ============新增绘制起点绿色圆点============
const start = this.routePoints[0];
ctx.beginPath();
ctx.fillStyle="#00c853";
//圆点半径
ctx.arc(start.x,start.y,12,0,2*Math.PI);
ctx.fill();

//可选：终点红色圆点
const end = this.routePoints[1];
ctx.beginPath();
ctx.fillStyle="#f44336";
ctx.arc(end.x,end.y,12,0,2*Math.PI);
ctx.fill();
    ctx.restore()
  },

  drawParking(ctx, x, y, w, h, text) {
    ctx.fillStyle = "rgba(255, 255, 255, 0.12)";
    ctx.fillRect(x, y, w, h);
    ctx.strokeStyle = "#ffffff"
    ctx.lineWidth = 2
    ctx.strokeRect(x, y, w, h)
    if (text != null && text != undefined && text.trim() !== "") {
      ctx.fillStyle = "#ffffff"
      ctx.font = "28px sans-serif"
      ctx.textAlign = "center"
      ctx.fillText(text, x + w / 2, y + h / 2 + 10)
    }
  },

  getDistance(x1, y1, x2, y2) {
    let dx = x2 - x1
    let dy = y2 - y1
    return Math.sqrt(dx * dx + dy * dy)
  },

  //==== 新增导航点击事件 ====
  startNav() {
    console.log("点击开始导航，绘制寻车路线");
    this.showRoute = true;
    this.drawMap();
  },

  touchStart(e) {
    if (e.touches.length === 2) {
      this.isTwoFinger = true
      let t1 = e.touches[0]
      let t2 = e.touches[1]
      this.lastDistance = this.getDistance(t1.x, t1.y, t2.x, t2.y)
    } else {
      this.isTwoFinger = false
      const fingerX = e.touches[0].x
      const fingerY = e.touches[0].y
      this.dragOffsetX = fingerX - this.translateX
      this.dragOffsetY = fingerY - this.translateY
    }
  },

  touchMove(e) {
    if (e.touches.length === 2) {
      let t1 = e.touches[0]
      let t2 = e.touches[1]
      let dist = this.getDistance(t1.x, t1.y, t2.x, t2.y)
      let centerX = (t1.x + t2.x) / 2
      let centerY = (t1.y + t2.y) / 2
      let scaleRatio = dist / this.lastDistance
      // 双指中心点缩放
      this.translateX = centerX - (centerX - this.translateX) * scaleRatio
      this.translateY = centerY - (centerY - this.translateY) * scaleRatio
      this.scale *= scaleRatio
      // 缩放极值锁定
      if (this.scale < 0.15) this.scale = 0.15
      if (this.scale > 2.5) this.scale = 2.5
      this.lastDistance = dist
    } else if (e.touches.length === 1 && !this.isTwoFinger) {
      let cx = e.touches[0].x
      let cy = e.touches[0].y
      this.translateX = cx - this.dragOffsetX
      this.translateY = cy - this.dragOffsetY
    }
    // 边界限制：防止画布被拖走消失
    const maxOffsetX = this.canvasW * this.scale * 0.6;
    const maxOffsetY = this.canvasH * this.scale * 0.6;
    this.translateX = Math.max(-maxOffsetX, Math.min(maxOffsetX, this.translateX))
    this.translateY = Math.max(-maxOffsetY, Math.min(maxOffsetY, this.translateY))
    // 简易节流防闪烁，代替requestAnimationFrame
    if (this.isDrawing) return;
    this.isDrawing = true;
    setTimeout(() => {
      this.drawMap()
      this.isDrawing = false;
    }, 16)
  },

  touchEnd() {
    this.isTwoFinger = false
  },
 
})
