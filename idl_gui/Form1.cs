/*
*   Arayüz ve kestirim yapılmıştır. Gelen uzaklık bilgilerinden ve konum bilgilerinden
*   çember denklemi çıkarıp kesişim noktaları bulunmuş, her üç çemberin de kesiştiği 
*   noktalar (3) üçgen çizizlip ağırlık merkezi alınmıştır.

*/


using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Printing;
using System.Drawing.Design;

using System.Threading;
using System.Diagnostics;
using System.IO;



namespace idl_ebru
{
    public partial class Form1 : Form
    {
        // DEGISKENLER 
        string serialPortData;
        string[] portlar = SerialPort.GetPortNames();

        float[] distances = { 0, 0, 0 };
        float[] perimeters = new float[8];
        float minimum;
        float R0, R1, R2;

        // CHANGE 390
        PointF BS0 = new Point(1380, 690); 
        PointF BS1 = new Point(1380, 300);
        PointF BS2 = new Point(990, 690);
        Point TagPosTOF = new Point(0, 0);
        Point ClickCoordinate = new Point(0, 0);
        PointF P1, P2, P3, P4, P5, P6, G;

        PointF[] centerG = new PointF[8];
        PointF[] noktalar = new PointF[24];


        int minimumIdx = 0;
        int est_err;
        int GridSize = 25;
        int ScaleVar = 2;



        private void Button5_Click(object sender, EventArgs e)
        {
            GridSize = Convert.ToUInt16(textBox4.Text);
        }

        private void TextBox1_Click(object sender, EventArgs e)
        {
            textBox1.Text = "";
        }

        private void Button2_Click(object sender, EventArgs e)
        {
            BS0.X = ClickCoordinate.X;
            BS0.Y = ClickCoordinate.Y;
        }

        private void Button3_Click(object sender, EventArgs e)
        {
            BS1.X = ClickCoordinate.X;
            BS1.Y = ClickCoordinate.Y;
        }

        private void Button4_Click(object sender, EventArgs e)
        {
            BS2.X = ClickCoordinate.X;
            BS2.Y = ClickCoordinate.Y;
        }

        private void Button6_Click(object sender, EventArgs e)
        {
            ScaleVar = Convert.ToUInt16(textBox1.Text);
        }

        private void TextBox4_Click(object sender, EventArgs e)
        {
            textBox4.Text = "";
        }

        private int TwoPointDistance(int x1, int y1, int x2, int y2) {
            return (int)Math.Sqrt( Math.Pow(x1-x2, 2) - Math.Pow(y1-y2, 2 ) );
        }

        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {

            foreach (string port in portlar)
            {
                comboBox1.SelectedIndex = 0;
            }
        }
        private void Button1_Click(object sender, EventArgs e)
        {
            try
            {
                if (button1.Text == "CONNECT")
                {
                    button1.Text = "DISCONNECT";
                    timer1.Start();
                }
                else
                {
                    button1.Text = "CONNECT";
                    timer1.Stop();
                }
            }
            catch
            {
            }
        }
        private void Timer1_Tick(object sender, EventArgs e)
        {
            try
            {
                label4.Text = "Birim uzunluk(grid) => " + Convert.ToString(ScaleVar * GridSize) + " cm";
            }
            catch
            {
            }
        }

        private void Timer2_Tick(object sender, EventArgs e)
        {
            try
            {
                DrawDefaultMap();
            }
            catch
            {
            }
        }

        private void DrawDefaultMap()
        {
            if (R0 != 0 && R1 != 0 && R2 != 0)
            {

                Bitmap drawingSurface = new Bitmap(2000, 1000);
                Graphics GFX = Graphics.FromImage(drawingSurface);
                GFX.SmoothingMode = SmoothingMode.AntiAlias;
                GFX.InterpolationMode = InterpolationMode.HighQualityBicubic;
                GFX.PixelOffsetMode = PixelOffsetMode.HighQuality;
                
                for (int i = 0; i <= 2000; i += GridSize)
                {
                    GFX.DrawLine(Pens.LightGray, i, 0, i, 1000);
                }

                for (int i = 0; i <= 1000; i += GridSize)
                {
                    GFX.DrawLine(Pens.LightGray, 0, i, 2000, i);
                }

                
                //GFX.FillRectangle(Brushes.Black, P1.X - 3, P1.Y - 3, 6, 6); GFX.DrawString("P1 = " + Convert.ToString(P1), new Font("Tahoma", 10), Brushes.Black, P1);
                GFX.FillRectangle(Brushes.Black, P1.X - 3, P1.Y - 3, 6, 6); GFX.DrawString("P1", new Font("Tahoma", 10), Brushes.Black, P1);
                GFX.FillRectangle(Brushes.Black, P2.X - 3, P2.Y - 3, 6, 6); GFX.DrawString("P2", new Font("Tahoma", 10), Brushes.Black, P2);
                GFX.FillRectangle(Brushes.Black, P3.X - 3, P3.Y - 3, 6, 6); GFX.DrawString("P3", new Font("Tahoma", 10), Brushes.Black, P3);
                GFX.FillRectangle(Brushes.Black, P4.X - 3, P4.Y - 3, 6, 6); GFX.DrawString("P4", new Font("Tahoma", 10), Brushes.Black, P4);
                GFX.FillRectangle(Brushes.Black, P5.X - 3, P5.Y - 3, 6, 6); GFX.DrawString("P5", new Font("Tahoma", 10), Brushes.Black, P5);
                GFX.FillRectangle(Brushes.Black, P6.X - 3, P6.Y - 3, 6, 6); GFX.DrawString("P6", new Font("Tahoma", 10), Brushes.Black, P6);
                
                pictureBox1.Image = drawingSurface;
                //label4.Text = Convert.ToString(R0);
            }
        }

        private void PictureBox1_MouseClick(object sender, MouseEventArgs e)
        {
            try
            {
                Bitmap b = new Bitmap(pictureBox1.Image);
                Color color = b.GetPixel(e.X, e.Y);
                label3.Text = "Click Coordinate => " + Convert.ToString(e.X) + " " + Convert.ToString(e.Y);
                ClickCoordinate.X = e.X;
                ClickCoordinate.Y = e.Y;
            }
            catch
            {
            }
        }
    }
}
