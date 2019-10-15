using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class Program : MonoBehaviour {

    public Button btnConnect;
    public Button btnApply;
    public InputField inputDevice;
    public InputField inputAp;
    public InputField inputPw;
    public Dropdown dropdownGeo;

    public void ScanDevice()
    {
        BTManager.Instance.Scan();
    }

    public void ConnectDevice()
    {
        BTManager.Instance.Connect(inputDevice.text);
    }

    public void ApplySetting()
    {
        StartCoroutine(Setting());
    }

    private IEnumerator Setting()
    {
        BTManager.Instance.Send("ap=" + inputAp.text);
        yield return new WaitForSeconds(0.1f);
        BTManager.Instance.Send("pw=" + inputPw.text);
        yield return new WaitForSeconds(0.1f);
        BTManager.Instance.Send("mo=0");
        yield return new WaitForSeconds(0.1f);
        BTManager.Instance.Send("si=" + dropdownGeo.options[dropdownGeo.value].text);
        yield return new WaitForSeconds(0.1f);
        BTManager.Instance.Send("re");
    }

    // Use this for initialization
    void Start () {
		
	}
	
	// Update is called once per frame
	void Update () {
        inputDevice.text = BTManager.Instance.GetDeviceAddress();

        if (BTManager.Instance.IsConnected())
        {
            btnConnect.GetComponentInChildren<Text>().text = "Disconnect";
            btnApply.interactable = true;
        }
        else
        {
            btnConnect.GetComponentInChildren<Text>().text = "Connect";
            btnApply.interactable = false;
        }
    }
}
