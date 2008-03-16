import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.ChoiceGroup;
import javax.microedition.lcdui.StringItem;

import javax.microedition.io.Connector;

import javax.bluetooth.DeviceClass;
import javax.bluetooth.DiscoveryAgent;
import javax.bluetooth.DiscoveryListener;
import javax.bluetooth.L2CAPConnection;
import javax.bluetooth.LocalDevice;
import javax.bluetooth.RemoteDevice;
import javax.bluetooth.ServiceRecord;

import javax.microedition.midlet.MIDlet;

public class L2cap extends MIDlet implements CommandListener, Runnable,
       DiscoveryListener
{
	private Canvas canvas;
	private Form form;
	private StringItem statusForm;
	private ChoiceGroup devices;
	private Command c_exit, c_dconn, c_stop, c_ok, c_inq;
	private L2CAPConnection iconn = null, cconn = null;
/*	private Thread thr_read = null;*/
	protected String statusCanvas = "", info = "";
	private byte incr;

	public L2cap() {
		canvas = new Canvas() {
			protected void paint(Graphics g) {
				Font f = g.getFont();
				g.setColor(255, 255, 55);
				g.fillRect(0, 0, this.getWidth(),
						f.getHeight());
				g.fillRect(0, this.getHeight() - f.getHeight(),
						this.getWidth(), f.getHeight());
				g.setColor(0, 0, 0);
				g.drawString(info, 0, 0,
						Graphics.TOP|Graphics.LEFT);
				g.drawString(statusCanvas, 0,
					this.getHeight() - f.getHeight(),
					Graphics.TOP|Graphics.LEFT);
			}
			protected void keyPressed(int keyCode) {
				send_cmd(keyCode, true);
			}
			protected void keyRepeated(int keyCode) {
				send_cmd(keyCode, false);
			}
		};

		canvas.addCommand(c_dconn = new Command("Disconnect",
					Command.CANCEL, 10));
		canvas.addCommand(c_exit = new Command("Exit",
					Command.EXIT, 100));
		canvas.setCommandListener(this);

		form = new Form("l2cap");
		form.append(statusForm = new StringItem("Status", null));
		form.append(devices = new ChoiceGroup("BT devices",
					ChoiceGroup.EXCLUSIVE));
		form.addCommand(c_inq = new Command("Inquiry",
					Command.ITEM, 2));
		form.addCommand(c_exit);
		form.setCommandListener(this);
		c_ok = new Command("OK", Command.OK, 1);
		c_stop = new Command("Stop", Command.CANCEL, 2);
	}

	private void setStatus(String status) {
		if (Display.getDisplay(this).getCurrent() == form) {
			statusForm.setText(status);
		} else {
			statusCanvas = status;
			canvas.repaint();
		}
	}

	private boolean connect(String baddr) {
		setStatus("connecting (" + baddr + ")");
		try {
			cconn = (L2CAPConnection)
				Connector.open("btl2cap://" + baddr + ":1011");
			iconn = (L2CAPConnection)
				Connector.open("btl2cap://" + baddr+ ":1013");
		} catch (Exception e) {
			setStatus("failed: " + e);
			disconnect();
			return false;
		} 
		setStatus("connected");
		return true;
	}

	private void send(byte buf[]) {
		if (iconn == null)
			return;
		try {
			iconn.send(buf);
		} catch (Exception e) {
			setStatus("send failed: " + e);
			return;
		}
		setStatus("sent");
	}

	protected void send_cmd(int keyCode, boolean first) {
		int game;

		if (first)
			incr = 5;
		else if (incr < 250)
			incr += 5;

		game = canvas.getGameAction(keyCode);
		if (game > 0) {
			byte buf[] = new byte[6];

			buf[0] = (byte)0xa1; /* DATA | INPUT */
			buf[1] = 0x02; /* mouse */
			buf[2] = 0x00; /* but */
			buf[3] = 0; /* X */
			buf[4] = 0; /* Y */
			buf[5] = 0; /* wheel */

			if (game == canvas.FIRE)
				buf[2] |= 0x1; /* LEFT */
			else if (game == canvas.GAME_A)
				buf[2] |= 0x2; /* RIGHT */
			else if (game == canvas.GAME_B)
				buf[2] |= 0x4; /* MIDDLE */
			else if (game == canvas.LEFT)
				buf[3] = (byte)-incr;
			else if (game == canvas.RIGHT)
				buf[3] = (byte)incr;
			else if (game == canvas.UP)
				buf[4] = (byte)-incr;
			else if (game == canvas.DOWN)
				buf[4] = (byte)incr;
			else if (game == canvas.GAME_C)
				buf[5] = (byte)-1;
			else if (game == canvas.GAME_D)
				buf[5] = 1;

			info = Integer.toString(keyCode) + " -> " +
				Integer.toString(game) + " -> 0x" +
				Integer.toHexString(buf[2]) + " 0x" +
				Integer.toHexString(buf[3]) + " 0x" +
				Integer.toHexString(buf[4]) + " 0x" +
				Integer.toHexString(buf[5]);
			canvas.repaint();

			send(buf);
			if (buf[2] != 0) { /* send button release */
				buf[2] = 0;
				send(buf);
			}
		} else {
			info = "key: " + Integer.toString(keyCode);
			canvas.repaint();
		}
	}

	private void disconnect() {
		try {
			if (iconn != null)
				iconn.close();
			if (cconn != null)
				cconn.close();
			iconn = cconn = null;
		} catch (Exception e) {
			setStatus("close failed: " + e);
			return;
		}
		setStatus("disconnected");
	}

/*	private void dumpBuf(byte buf[], int maxlen) {
		StringBuffer sb = new StringBuffer(maxlen * 3);
		int a;

		if (buf.length < maxlen)
			maxlen = buf.length;

		for (a = 0; a < maxlen; a++)
			sb.append(Integer.toHexString(buf[a]));

		info = sb.toString();
		canvas.repaint();
	}

	private void read() {
		while (true) {
			try {
				Thread.currentThread().sleep(1000);
				while (cconn.ready()) {
					byte buf[] = new byte[256];
					int len = cconn.receive(buf);
					dumpBuf(buf, 10);
				}
			} catch (Exception e) {
				setStatus("failed: " + e);
				return;
			}
		}
	}*/

	public void startApp() {
		Display.getDisplay(this).setCurrent(form);
	}

	public void pauseApp() {
	}

	public void destroyApp(boolean unconditional) {
/*		if (thr_read != null)
			thr_read.interrupt();*/
		stopInquiry(true);
		disconnect();
	}

	private void inquiry() {
		devices.deleteAll();
		form.removeCommand(c_ok);
		form.removeCommand(c_inq);
		try {
			setStatus("inquiry started");
			LocalDevice.getLocalDevice().
				getDiscoveryAgent().
				startInquiry(DiscoveryAgent.GIAC, this);
		} catch (Exception e) {
			setStatus("inquiry: " + e);
			return;
		}
		form.addCommand(c_stop);
	}

	private void stopInquiry(boolean hard) {
		if (hard) {
			try {
				LocalDevice.getLocalDevice().
					getDiscoveryAgent().
					cancelInquiry(this);
			} catch (Exception e) {
				return;
			}
		}
		form.removeCommand(c_stop);
		if (devices.size() == 0) {
			setStatus("no devices found");
		} else {
			setStatus("inquiry finished");
			form.addCommand(c_ok);
		}
		form.addCommand(c_inq);
	}

	public void run() {
		switch (Integer.parseInt(Thread.currentThread().getName())) {
		case 0:
			Display.getDisplay(this).setCurrent(canvas);
			if (!connect(devices.getString(
					devices.getSelectedIndex()))) {
				Display.getDisplay(this).setCurrent(form);
				break;
			}
/*			if (thr_read == null)
				thr_read = new Thread(this, "100");*/
			break;
		case 1:
/*			thr_read.interrupt();
			thr_read = null;*/
			Display.getDisplay(this).setCurrent(form);
			disconnect();
			break;
/*		case 100:
			read();
			break;*/
		}
	}

	public void commandAction(Command c, Displayable d) {
		if (c == c_exit) {
			destroyApp(false);
			notifyDestroyed();
			return;
		}
		if (d == form) {
			if (c == c_inq)
				inquiry();
			else if (c == c_stop)
				stopInquiry(true);
			else if (c == c_ok)
				new Thread(this, "0").start();
			return;
		}
		if (c == c_dconn)
			new Thread(this, "1").start();
	}

	public void deviceDiscovered(RemoteDevice btDevice, DeviceClass cod) {
		devices.append(btDevice.getBluetoothAddress(), null);
	}

	public void inquiryCompleted(int discType) {
		stopInquiry(false);
	}

	public void servicesDiscovered(int transID,
			ServiceRecord[] servRecord) {
	}

	public void serviceSearchCompleted(int transID, int respCode) {
	}
}
