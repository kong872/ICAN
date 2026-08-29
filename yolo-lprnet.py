import cv2
import time
import numpy as np
from rknnpool.rknnpool_ld import rknnPoolExecutor
from func.func_yolov8_optimize import myFunc
from rknnlite.api import RKNNLite
import struct
import os
from collections import Counter

# ===================== LPRNET 车牌识别配置 =====================
CHARS = [
    '京','沪','津','渝','冀','晋','蒙','辽','吉','黑',
    '苏','浙','皖','闽','赣','鲁','豫','鄂','湘','粤',
    '桂','琼','川','贵','云','藏','陕','甘','青','宁',
    '新','0','1','2','3','4','5','6','7','8','9',
    'A','B','C','D','E','F','G','H','J','K','L','M','N',
    'P','Q','R','S','T','U','V','W','X','Y','Z','I','O','-'
]
BLANK_IDX = len(CHARS) - 1

def ctc_decode(preds):
    pred = preds[0]
    res = []
    last = -1
    for t in range(pred.shape[1]):
        idx = np.argmax(pred[:, t])
        if idx != BLANK_IDX and idx != last:
            res.append(idx)
            last = idx
        if len(res) >= 7:
            break
    return ''.join([CHARS[i] for i in res])

# ===================== 车位状态全局变量 =====================
last_parking_state = {
    "space_1": -1, "space_2": -1, "space_3": -1, "space_4": -1,
    "space_5": -1, "space_6": -1, "space_7": -1, "space_8": -1
}

# ============滑动投票防抖参数============
HISTORY_MAX = 8             # 保存最近8帧识别记录
VOTE_THRESHOLD = 4          # 同一个车牌出现4次即判定成功
MIN_PLATE_LEN = 5           # 小于5字符直接丢弃乱码

# 每个车位：历史列表 + 已经确认打印的车牌
plate_data = {
    "space_1":{"history":[], "printed":None},
    "space_2":{"history":[], "printed":None},
    "space_3":{"history":[], "printed":None},
    "space_4":{"history":[], "printed":None},
    "space_5":{"history":[], "printed":None},
    "space_6":{"history":[], "printed":None},
    "space_7":{"history":[], "printed":None},
    "space_8":{"history":[], "printed":None},
}

DEBUG_PRINT = True

def get_plate_parking_space(cx, cy, parking_dict):
    for space_name, polygon in parking_dict.items():
        ret = cv2.pointPolygonTest(polygon, (cx, cy), False)
        if ret >= 0:
            return space_name
    return None

def send_all_num_to_oled1(num: int):
    buf = struct.pack("i", num)
    try:
        fd = os.open("/temp/oled1_fifo", os.O_WRONLY | os.O_NONBLOCK)
        os.write(fd, buf)
        os.close(fd)
    except Exception:
        pass

def send_oled2_data(l1, s1, r1):
    buf = struct.pack("iii", l1, s1, r1)
    try:
        fd = os.open("/temp/oled2_fifo", os.O_WRONLY | os.O_NONBLOCK)
        os.write(fd, buf)
        os.close(fd)
    except Exception:
        pass

def send_oled3_data(l2, r2):
    buf = struct.pack("ii", l2, r2)
    try:
        fd = os.open("/temp/oled3_fifo", os.O_WRONLY | os.O_NONBLOCK)
        os.write(fd, buf)
        os.close(fd)
    except Exception:
        pass

def send_light_bit_data(bit_byte: int):
    buf = struct.pack("B", bit_byte)
    try:
        fd = os.open("/temp/led_fifo", os.O_WRONLY | os.O_NONBLOCK)
        os.write(fd, buf)
        os.close(fd)
    except Exception:
        pass

def send_pwm_fifo_total(all_empty: int):
    buf = struct.pack("i", all_empty)
    try:
        fd = os.open("/temp/pwm_fifo", os.O_WRONLY | os.O_NONBLOCK)
        os.write(fd, buf)
        os.close(fd)
    except Exception:
        pass

# ===================== 主程序入口 =====================
if __name__ == "__main__":
    out_win = "output_style_full_screen"
    cap = cv2.VideoCapture("/dev/video52")
    modelPath = "./rknnModel/t.rknn"
    lpr_rknn_path = "/home/elf/code/ai/lprnet/lprnet.rknn"
    TPEs = 8
    LEFT_SPACES1 = ["space_4"]
    STRAIGHT_SPACES1 = ["space_5", "space_6", "space_7", "space_8"]
    RIGHT_SPACES1 = ["space_1", "space_2", "space_3"]
    LEFT_SPACES2 = ["space_8"]
    RIGHT_SPACES2 = ["space_5", "space_6", "space_7"]
    PARKING_SPACES = {
        "space_1": np.array([[244, 257], [187, 307], [285, 306], [325, 258]]),
        "space_2": np.array([[325, 258], [282, 306], [382, 303], [404, 257]]),
        "space_3": np.array([[404, 257], [382, 301], [479, 301], [484, 256]]),
        "space_4": np.array([[600, 251], [622, 298], [722, 295], [680, 245]]),
        "space_5": np.array([[143, 341], [18, 446], [169, 444], [254, 341]]),
        "space_6": np.array([[254, 339], [169, 443], [320, 440], [369, 339]]),
        "space_7": np.array([[369, 339], [319, 438], [472, 438], [479, 338]]),
        "space_8": np.array([[636, 331], [691, 435], [850, 436], [754, 330]])
    }
    pool = rknnPoolExecutor(
        rknnModel=modelPath,
        TPEs=TPEs,
        func=myFunc
    )
    lpr_rknn = RKNNLite()
    lpr_rknn.load_rknn(lpr_rknn_path)
    lpr_rknn.init_runtime()
    print("LPRNet模型加载完成")

    if cap.isOpened():
        for i in range(TPEs + 1):
            ret, frame = cap.read()
            if not ret:
                cap.release()
                del pool
                lpr_rknn.release()
                exit(-1)
            pool.put(frame)

    frames, loopTime, initTime = 0, time.time(), time.time()
    fps_start_time = time.time()
    fps_frame_count = 0
    last_fps = 0.0

    while cap.isOpened():
        frames += 1
        ret, frame = cap.read()
        if not ret:
            break
        pool.put(frame)
        result, flag = pool.get()
        if flag == False:
            break
        frame, boxes, classes = result
        raw_h, raw_w = frame.shape[:2]
        show_w = 1024
        show_h = 600
        scale_x_show = show_w / raw_w
        scale_y_show = show_h / raw_h
        show_frame = cv2.resize(frame, (show_w, show_h))
        car_points = []

        if boxes is not None:
            for i in range(len(boxes)):
                class_id = int(classes[i])
                box = boxes[i]
                xmin, ymin, xmax, ymax = box[0], box[1], box[2], box[3]
                if class_id == 0:
                    Px = int((xmin + xmax) / 2)
                    Py = int((ymin + ymax) / 2)
                    Px_roi = int(Px * scale_x_show)
                    Py_roi = int(Py * scale_y_show)
                    car_points.append((Px_roi, Py_roi))
                    Px_show = int(Px * scale_x_show)
                    Py_show = int(Py * scale_y_show)
                    cv2.circle(show_frame, (Px_show, Py_show), 5, (0, 0, 255), -1)

                elif class_id == 1:
                    box_w = xmax - xmin
                    box_h = ymax - ymin
                    x1 = max(0, int(xmin - box_w * 0.15))
                    y1 = max(0, int(ymin - box_h * 0.15))
                    x2 = min(raw_w, int(xmax + box_w * 0.15))
                    y2 = min(raw_h, int(ymax + box_h * 0.15))

                    plate_roi = frame[y1:y2, x1:x2]
                    if plate_roi.size == 0:
                        continue

                    plate_img = cv2.resize(plate_roi, (188, 48), interpolation=cv2.INTER_CUBIC)
                    kernel_sharpen = np.array([
                        [0, -1, 0],
                        [-1, 5, -1],
                        [0, -1, 0]
                    ])
                    plate_img = cv2.filter2D(plate_img, -1, kernel_sharpen)
                    plate_img = cv2.resize(plate_img, (94, 24))

                    plate_input = np.expand_dims(plate_img, axis=0)
                    outputs = lpr_rknn.inference([plate_input])
                    plate_str = ctc_decode(outputs[0])

                    plate_cx_show = int((xmin + xmax)/2 * scale_x_show)
                    plate_cy_show = int((ymin + ymax)/2 * scale_y_show)
                    current_space = get_plate_parking_space(plate_cx_show, plate_cy_show, PARKING_SPACES)

                    if DEBUG_PRINT:
                        print(f"[DEBUG]车位:{current_space},识别结果:{plate_str}")

                    if current_space is None:
                        continue
                    pd = plate_data[current_space]

                    # 丢弃过短无效结果
                    if len(plate_str) >= MIN_PLATE_LEN:
                        pd["history"].append(plate_str)
                        if len(pd["history"]) > HISTORY_MAX:
                            pd["history"].pop(0)

                        # 投票统计
                        cnt = Counter(pd["history"])
                        best_plate, vote_cnt = cnt.most_common(1)[0]

                        # 票数达标并且还没有打印过
                        if vote_cnt >= VOTE_THRESHOLD and pd["printed"] != best_plate:
                            pd["printed"] = best_plate
                            # ========只打印车牌，不带车位文字========
                            print(best_plate)

                    p_show_x = int((xmin + xmax) * 0.5 * scale_x_show)
                    p_show_y = int((ymin + ymax) * 0.5 * scale_y_show)
                    cv2.circle(show_frame, (p_show_x, p_show_y), 6, (255, 0, 0), -1)

        parking_status = {}
        for space_name, polygon in PARKING_SPACES.items():
            is_occupied = False
            for point in car_points:
                if cv2.pointPolygonTest(polygon, point, False) >= 0:
                    is_occupied = True
                    break
            parking_status[space_name] = 0 if is_occupied else 1
            if is_occupied:
                color = (0, 0, 255)
                status_text = "Occupied"
            else:
                color = (0, 255, 0)
                status_text = "Empty"
            cv2.polylines(show_frame, [polygon], True, color, 2)
            top_left = polygon[0]
            top_right = polygon[1]
            top_mid_x = int((top_left[0] + top_right[0]) / 2)
            top_mid_y = int((top_left[1] + top_right[1]) / 2)
            text = f"{space_name}: {status_text}"
            text_size = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 0.7, 2)[0]
            text_x = int(top_mid_x - text_size[0] / 2)
            text_y = int(top_mid_y - 10)
            cv2.putText(show_frame, text, (text_x, text_y),cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)

        for sp_name, curr_sta in parking_status.items():
            if curr_sta != last_parking_state[sp_name]:
                tip = "空余" if curr_sta == 1 else "已占用"
                print(f"【状态变更】{sp_name} -> {tip}")
                # 车辆开走清空缓存，新车进来可以再次识别打印
                if curr_sta == 1:
                    plate_data[sp_name]["history"].clear()
                    plate_data[sp_name]["printed"] = None
                last_parking_state[sp_name] = curr_sta

        left_num1 = sum(parking_status.get(s, 0) for s in LEFT_SPACES1)
        straight_num1 = sum(parking_status.get(s, 0) for s in STRAIGHT_SPACES1)
        right_num1 = sum(parking_status.get(s, 0) for s in RIGHT_SPACES1)
        send_oled2_data(left_num1, straight_num1, right_num1)
        all_num = sum(parking_status.get(s, 0) for s in PARKING_SPACES)
        send_all_num_to_oled1(all_num)
        send_pwm_fifo_total(all_num)
        left_num2 = sum(parking_status.get(s, 0) for s in LEFT_SPACES2)
        right_num2 = sum(parking_status.get(s, 0) for s in RIGHT_SPACES2)
        send_oled3_data(left_num2, right_num2)
        bit_data = 0
        space_order = ["space_1", "space_2", "space_3", "space_4", "space_5", "space_6", "space_7", "space_8"]
        for bit_idx, space_name in enumerate(space_order):
            val = parking_status.get(space_name, 0)
            if val == 1:
                bit_data |= (1 << bit_idx)
        send_light_bit_data(bit_data)

        cv2.putText(show_frame,f"Left : {left_num1}",(40, 50),cv2.FONT_HERSHEY_SIMPLEX,1,(0, 255, 0),2)
        cv2.putText(show_frame,f"Straight1 : {straight_num1}",(40, 100),cv2.FONT_HERSHEY_SIMPLEX,1,(0, 255, 255),2)
        cv2.putText(show_frame,f"Right1 : {right_num1}",(40, 150),cv2.FONT_HERSHEY_SIMPLEX,1,(255, 0, 0),2)
        cv2.putText(show_frame,f"Left2 : {left_num2}",(40, 200),cv2.FONT_HERSHEY_SIMPLEX,1,(0, 255, 0),2)
        cv2.putText(show_frame,f"Right2 : {right_num2}",(40, 250),cv2.FONT_HERSHEY_SIMPLEX,1,(255, 0, 0),2)
        cv2.putText(show_frame,f"ALL : {all_num}",(400, 50),cv2.FONT_HERSHEY_SIMPLEX,1,(255, 0, 0),2)

        now_time = time.time()
        fps_frame_count += 1
        delta = now_time - fps_start_time
        if delta >= 1.0:
            last_fps = fps_frame_count / delta
            fps_frame_count = 0
            fps_start_time = now_time

        cv2.putText(show_frame,f"FPS:{last_fps:.1f}",(620,50),cv2.FONT_HERSHEY_SIMPLEX,1,(0,255,255),2)
        cv2.namedWindow(out_win, cv2.WINDOW_NORMAL)
        cv2.setWindowProperty(out_win, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)
        cv2.imshow(out_win, show_frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
        if frames % 30 == 0:
            loopTime = time.time()

    cap.release()
    cv2.destroyAllWindows()
    pool.release()
    lpr_rknn.release()
