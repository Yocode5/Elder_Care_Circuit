using System.IO.Ports;
using UnityEngine;

public class MPUReceiver : MonoBehaviour
{
    SerialPort stream = new SerialPort("COM12", 115200);

    float gx, gy, gz;

    void Start()
    {
        stream.ReadTimeout = 50;
        stream.Open();
    }

    void Update()
    {
        if (stream.IsOpen)
        {
            try
            {
                string data = stream.ReadLine();
                string[] v = data.Split(',');

                if (v.Length == 8)
                {
                    gx = float.Parse(v[3]);
                    gy = float.Parse(v[4]);
                    gz = float.Parse(v[5]);

                    ApplyRotation();
                }
            }
            catch {}
        }
    }

    void ApplyRotation()
    {
        // smoother rotation
        transform.Rotate(gx * Time.deltaTime, gy * Time.deltaTime, gz * Time.deltaTime);
    }

    void OnApplicationQuit()
    {
        if (stream.IsOpen)
            stream.Close();
    }
}