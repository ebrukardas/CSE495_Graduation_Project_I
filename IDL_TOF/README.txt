# dwm1001-keil-tof

#3 BS, 1 TAG uygulama

Konum TOF ile bulunur.

Tag kodu direk yuklenir

Anchor kodlari yuklenirken Anchor_Idx isimli degiskenden anchor numarasi verilir

Ayarlarý: 115200 baud rate - databits 8 - stop bir one - parity none - flow control xon/xoff

#

Proje yapýsý: 
```
dwm1001-examples-master/
+¦¦ boards            // DWM1001-DEV karta ozel tanimlamalar
+¦¦ deca_driver       // DW1000 API yazilim paketi 2.04 
+¦¦ examples          //
-   +¦¦ ANCHORS       //
-   +¦¦ TAG           //
-   +¦¦ GATEWAY       //
+¦¦ nRF5_SDK_14.2.0   // Nordic Semiconductor SDK 14.2