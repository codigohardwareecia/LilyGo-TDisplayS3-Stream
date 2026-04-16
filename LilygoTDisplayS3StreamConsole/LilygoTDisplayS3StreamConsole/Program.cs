using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Net.Sockets;
using System.Text;

string esp32Ip = "192.168.15.132";
TcpClient client = new TcpClient(esp32Ip, 4210);
NetworkStream stream = client.GetStream();

int targetWidth = 320;
int targetHeight = 170;

while (client.Connected)
{
    // Ajuste para sua resolução se necessário
    using Bitmap screenshot = new Bitmap(1920, 1080);
    using (Graphics g = Graphics.FromImage(screenshot))
    {
        g.CopyFromScreen(0, 0, 0, 0, screenshot.Size);
    }

    // 2. Redimensiona para o tamanho do LCD do S3
    using Bitmap resized = new Bitmap(targetWidth, targetHeight);
    using (Graphics g = Graphics.FromImage(resized))
    {
        // O SEGREDO ESTÁ AQUI:
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.SmoothingMode = SmoothingMode.HighQuality;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.CompositingQuality = CompositingQuality.HighQuality;

        g.DrawImage(screenshot, new Rectangle(0, 0, targetWidth, targetHeight));
    }

    byte[] img = GetJpegBytes(resized, 40); // Qualidade 50 (fica nítido)

    // Manda o tamanho da imagem primeiro (4 bytes) para o ESP saber quanto ler
    byte[] size = BitConverter.GetBytes(img.Length);
    stream.Write(size, 0, 4);

    // Manda a imagem
    stream.Write(img, 0, img.Length);
    Thread.Sleep(1);
}
static byte[] GetJpegBytes(Bitmap bitmap, long quality)
{
    EncoderParameters encoderParameters = new EncoderParameters(1);
    encoderParameters.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, quality);
    ImageCodecInfo codecInfo = ImageCodecInfo.GetImageDecoders().First(c => c.FormatID == ImageFormat.Jpeg.Guid);

    using MemoryStream ms = new MemoryStream();
    bitmap.Save(ms, codecInfo, encoderParameters);
    return ms.ToArray();
}
