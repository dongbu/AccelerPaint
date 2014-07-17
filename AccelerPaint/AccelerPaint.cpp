#include "AccelerPaint.h"
#include "wx/wx.h"
#include "wx/dcbuffer.h"
#include "wx/scrolbar.h"
#include <wx/msgdlg.h>
#include <wx/intl.h>
#include <wx/string.h>
#include <wx/wfstream.h>
#include <sstream>

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _DEBUG
#define OPENCL_TEST
#endif


const long AccelerPaint::ID_OpenItem = wxNewId();
const long AccelerPaint::ID_OpenLItem = wxNewId();
const long AccelerPaint::ID_SaveItem = wxNewId();
const long AccelerPaint::ID_Layer = wxNewId();
const long AccelerPaint::ID_HScroll = wxNewId();
const long AccelerPaint::ID_VScroll = wxNewId();

BEGIN_EVENT_TABLE(AccelerPaint,wxFrame)
    //(*EventTable(IFXFrame)
    //*)
END_EVENT_TABLE()


AccelerPaint::AccelerPaint(wxWindow* parent,wxWindowID id)
{
  //Build the GUI
  opencl_img = NULL;
  Create_GUI(parent,id);
  SetTitle("AccelerPaint");

  //Help prevent flickering on windows.
#ifdef _WINDOWS
  SetDoubleBuffered(true);
#endif
}

void AccelerPaint::Create_GUI(wxWindow* parent, wxWindowID id)
{
  //Set up the Main application window
  wxPoint Selection_Point(15,0);

  Create(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE|wxNO_BORDER, _T("wxID_ANY"));
  
  wxSize minimum_size(Program_Min_Width, Program_Min_Height);
  SetMinSize(minimum_size);
  SetSize(minimum_size);

  //Create subwindows
  Create_GUI_MenuStrip(parent, id);
  Create_GUI_Tools(parent, id);
  Create_GUI_Layers(parent, id);
  Create_GUI_ImagePanel(parent, id);

  //Connect Window Events
  Connect(wxEVT_SIZE, (wxObjectEventFunction)&AccelerPaint::ResizeWindow);
}
void AccelerPaint::Create_GUI_MenuStrip(wxWindow* parent, wxWindowID id)
{
  //Components
  wxMenuItem* menu_items;
  menustrip = new wxMenuBar();

  //Construct File Menu
  filemenu = new wxMenu();
  menu_items = new wxMenuItem(filemenu, ID_OpenItem, _("Open"), wxEmptyString, wxITEM_NORMAL);
  filemenu->Append(menu_items);
  menu_items = new wxMenuItem(filemenu, ID_OpenLItem, _("Open Layer"), wxEmptyString, wxITEM_NORMAL);
  filemenu->Append(menu_items);
  menu_items = new wxMenuItem(filemenu, ID_SaveItem, _("Save"), wxEmptyString, wxITEM_NORMAL);
  filemenu->Append(menu_items);
  menustrip->Append(filemenu, _("File"));

  //Construct Filter Menu
  filemenu = new wxMenu();
  int ID_Blur = wxNewId();
  menu_items = new wxMenuItem(filemenu, ID_Blur, _("Blur"), wxEmptyString, wxITEM_NORMAL);
  filemenu->Append(menu_items);
  menustrip->Append(filemenu, _("Filter"));
  
  //Set the Menu bar
  SetMenuBar(menustrip);

  //Connect Events for Menustrip
  Connect(ID_OpenItem, wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&AccelerPaint::OpenFile);
  Connect(ID_OpenLItem, wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&AccelerPaint::OpenLayer);
  Connect(ID_SaveItem, wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&AccelerPaint::SaveRender);

  //Filters
  Connect(ID_Blur, wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&AccelerPaint::BlurLayer);


}
void AccelerPaint::Create_GUI_Layers(wxWindow* parent, wxWindowID id)
{
  //Construct Layers
  layerframe = new wxPanel(this, wxNewId(), wxPoint(0,0), wxSize(LAYERFRAME_WIDTH + 7,LAYERFRAME_HEIGHT + 7), wxRAISED_BORDER);
  layersinfo = new wxCheckListBox(layerframe, ID_Layer, wxPoint(0,26), wxSize(LAYERFRAME_WIDTH, LAYERFRAME_HEIGHT - 24));
  
  //Construct Layer Controls
  int ID_Spinctrl = wxNewId();
  new wxStaticText(layerframe, wxNewId(), "Opacity", wxPoint(0,4));
  opacityctrl = new wxSpinCtrl(layerframe, ID_Spinctrl, "100", wxPoint(50,1), wxSize(85, 22));

  //Connect Events for the layer checking
  Connect(ID_Layer, wxEVT_CHECKLISTBOX,(wxObjectEventFunction)&AccelerPaint::LayerChecked);
  Connect(ID_Layer, wxEVT_LISTBOX,(wxObjectEventFunction)&AccelerPaint::LayerChanged);
  Connect(ID_Spinctrl, wxEVT_SPINCTRL,(wxObjectEventFunction)&AccelerPaint::OpacityChanged);

}
void AccelerPaint::Create_GUI_ImagePanel(wxWindow* parent, wxWindowID id)
{
  //Create The actual work area
  imagepanel = new wxPanel(this, wxNewId(), wxPoint(toolspanel->GetSize().GetWidth(),0), 
                           wxSize(LAYERFRAME_WIDTH + 7,LAYERFRAME_HEIGHT + SCROLL_BORDER_SIZE));
  imagepanel->SetSize(this->GetSize().GetWidth() - LAYERFRAME_WIDTH - SCROLL_BORDER_SIZE - TOOLFRAME_WIDTH, 
               this->GetSize().GetHeight() - LAYERFRAME_HEIGHT - SCROLL_BORDER_SIZE - TOOLFRAME_HEIGHT);
  imagepanel->SetBackgroundStyle(wxBG_STYLE_PAINT);

  //Scroll bars for image
  imagesizeh = new wxScrollBar(imagepanel, ID_HScroll, wxPoint(0, 0), wxSize(imagepanel->GetSize().GetWidth(), 18));
  imagesizeh->Enable(false);

  imagesizev = new wxScrollBar(imagepanel, ID_VScroll, wxPoint(0, 0), wxSize(18, imagepanel->GetSize().GetHeight()), wxVERTICAL);
  imagesizev->Enable(false);

  //Create the new ImagePanel to handel OpenCL drawing
  //This is the component that does all the work.
  opencl_img = new Accel_ImagePanel(imagepanel, imagesizeh, imagesizev);

  //Connect part events  
  imagepanel->Connect(wxEVT_PAINT,(wxObjectEventFunction)&AccelerPaint::ImageBackground);
  Connect(ID_HScroll, wxEVT_SCROLL_CHANGED,(wxObjectEventFunction)&AccelerPaint::ImageScroll);
  Connect(ID_VScroll, wxEVT_SCROLL_CHANGED,(wxObjectEventFunction)&AccelerPaint::ImageScroll);

}
void AccelerPaint::Create_GUI_Tools(wxWindow* parent, wxWindowID id)
{
  //Construct SideFrame
  toolspanel = new wxPanel(this, wxNewId(), wxPoint(0,0), wxSize(TOOLFRAME_WIDTH,TOOLFRAME_HEIGHT), wxRAISED_BORDER);

  unsigned toolindex = 0;
  seperators = 0;
  Create_GUI_Tool_Generic(toolspanel, toolindex++, "./resources/arrow.png", "Move Tool");
  Create_GUI_Tool_Generic(toolspanel, toolindex++, "./resources/select.png", "Selection Tool");
  Create_GUI_Tool_Sperator(toolspanel, toolindex);
  Create_GUI_Tool_Generic(toolspanel, toolindex++, "./resources/brush.png", "Brush Tool");
  Create_GUI_Tool_Generic(toolspanel, toolindex++, "./resources/fill.png", "Fill Tool");
  Create_GUI_Tool_Sperator(toolspanel, toolindex);
  Create_GUI_Tools_Color(toolspanel, toolindex++);
  
  Toolsupdate(0);
}
void AccelerPaint::Create_GUI_Tool_Generic(wxWindow* parent, unsigned toolindex, const char* img_path, const char* tooltip)
{
  //Quite png loader
  wxLogNull Nolog;
  unsigned Button_event = wxNewId();
  wxImage img;
  img.LoadFile(img_path);
  wxBitmapButton* button = new wxBitmapButton(parent, Button_event, img, wxPoint(0,0), wxSize(0,0), wxBORDER_NONE);
  button->SetSize(TOOLFRAME_WIDTH - TOOLFRAME_BUFFER * 2, TOOLFRAME_WIDTH - TOOLFRAME_BUFFER * 2);
  button->SetPosition(wxPoint(1, (TOOLFRAME_WIDTH - TOOLFRAME_BUFFER - 3) * toolindex + TOOLFRAME_SEPERATOR * seperators));
  button->SetToolTip(new wxToolTip(tooltip));

  toolbuttons.push_back(button);

  Connect(Button_event, wxEVT_BUTTON,(wxObjectEventFunction)&AccelerPaint::ToolSelected);
}
void AccelerPaint::Create_GUI_Tool_Sperator(wxWindow* parent, unsigned toolindex)
{
  //Quite png loader
  wxLogNull Nolog;
  wxImage img;
  img.LoadFile("./resources/seperator.png");
  wxStaticBitmap* bitm = new wxStaticBitmap(parent, wxNewId(), wxBitmap(img));
  //img->SetSize(TOOLFRAME_WIDTH, TOOLFRAME_BUFFER - 2);
  bitm->SetPosition(wxPoint(1, (TOOLFRAME_WIDTH - TOOLFRAME_BUFFER - 3) * toolindex + 2 + TOOLFRAME_SEPERATOR * seperators));
  seperators++;
}
void AccelerPaint::Create_GUI_Tools_Color(wxWindow* parent, unsigned toolindex)
{
  unsigned Button_event = wxNewId();
  ColorButton = new wxButton(parent, Button_event, _(""), wxPoint(0,0), wxSize(0,0), wxBORDER_NONE );
  ColorButton->SetSize(TOOLFRAME_WIDTH - TOOLFRAME_BUFFER * 2 - 1, TOOLFRAME_WIDTH - TOOLFRAME_BUFFER * 2 - 2);
  ColorButton->SetPosition(wxPoint(1, (TOOLFRAME_WIDTH - TOOLFRAME_BUFFER - 3) * toolindex + 2 + TOOLFRAME_SEPERATOR * seperators));
  pickedcolor = *wxBLACK;
  ColorButton->SetBackgroundColour(pickedcolor);
  ColorButton->SetToolTip(new wxToolTip("Select Tool Color"));
  
  Connect(Button_event, wxEVT_BUTTON,(wxObjectEventFunction)&AccelerPaint::ColorPicker);
}
void AccelerPaint::ResizeWindow(wxSizeEvent& event)
{
  //Reset Layer Frame position
  layerframe->SetPosition(wxPoint(GetSize().GetWidth() - LAYERFRAME_WIDTH - 21, GetSize().GetHeight() - LAYERFRAME_HEIGHT - 64));
  imagepanel->SetSize(this->GetSize().GetWidth() - LAYERFRAME_WIDTH - 21 - TOOLFRAME_WIDTH, this->GetSize().GetHeight() - 58);
  
  imagesizev->SetPosition(wxPoint(imagepanel->GetSize().GetWidth() - SCROLL_SIZE_WIDTH, 0));
  imagesizev->SetSize(SCROLL_SIZE_WIDTH, imagepanel->GetSize().GetHeight() - SCROLL_SIZE_WIDTH);

  imagesizeh->SetPosition(wxPoint(0, imagepanel->GetSize().GetHeight() - SCROLL_SIZE_WIDTH));
  imagesizeh->SetSize(imagepanel->GetSize().GetWidth() - SCROLL_SIZE_WIDTH, SCROLL_SIZE_WIDTH);
}
void AccelerPaint::OpenFile(wxCommandEvent& event)
{
  //Simply call load dialog
  wxFileDialog  dlg( this, _T("Select a supported Image file"), wxEmptyString, wxEmptyString, _T("Acceler Source (*.acl)|*.acl|PNG files (*.png)|*.png|JPG files (*.jpg)|*.jpg|BMP files (*.bmp)|*.bmp|GIF files (*.gif)|*.gif"), wxFD_OPEN|wxFD_FILE_MUST_EXIST);


	if( dlg.ShowModal() == wxID_OK )
	{
    //load acceler files
    if(dlg.GetFilterIndex() == 0)
    {      
      wxFileInputStream input_stream(dlg.GetPath());
      if (!input_stream.IsOk())
      {
          wxLogError("Unable to aquire stream from acceler saving.", dlg.GetPath());
          return;
      }
      layersinfo->Clear();

      Acceler_Data datagram;
      input_stream.ReadAll(&datagram, sizeof(datagram));
      for(unsigned layer = 0; layer < datagram.layer_count; layer++)
      {
        unsigned char* data = new unsigned char[datagram.i_height * datagram.i_width * COLOR_DEPTH];
        unsigned char* alpha = new unsigned char[datagram.i_height * datagram.i_width];
        Layer_Data discriptor;
        input_stream.ReadAll(&discriptor, sizeof(discriptor));
        char name[200];
        int layer_size;
        input_stream.ReadAll(&layer_size, sizeof(layer_size));
        input_stream.ReadAll(name, layer_size);
        name[layer_size] = 0;
        input_stream.ReadAll(data, datagram.i_height * datagram.i_width * COLOR_DEPTH);
        input_stream.ReadAll(alpha, datagram.i_height * datagram.i_width);
        opencl_img->LoadFile(datagram.i_width, datagram.i_height, data, alpha, layer != 0);
        opencl_img->CheckVisability(layer, discriptor.visible);
        opencl_img->SetOpacity(layer, discriptor.opacity);
        
        layersinfo->Insert(name, layersinfo->GetCount());
        if(discriptor.visible)
          layersinfo->Check(layersinfo->GetCount() - 1);
      }
    }
    //load everything else
    else
    {
      opencl_img->LoadFile(dlg.GetPath());
      opencl_img->Refresh();

      layersinfo->Clear();
      layersinfo->Insert(dlg.GetFilename(), layersinfo->GetCount());
      layersinfo->Check(layersinfo->GetCount() - 1);
    }
  }
}
void AccelerPaint::OpenLayer(wxCommandEvent& event)
{
  //Return if an image isn't loaded yet
  if(opencl_img->LayerCount() == 0)
  {
    return;
  }
  //Simply call load dialog
  wxFileDialog  dlg( this, _T("Select a supported Image file"), wxEmptyString, wxEmptyString, _T("PNG files (*.png)|*.png|JPG files (*.jpg)|*.jpg|BMP files (*.bmp)|*.bmp|GIF files (*.gif)|*.gif"), wxFD_OPEN|wxFD_FILE_MUST_EXIST);

	if( dlg.ShowModal() == wxID_OK )
	{
    opencl_img->LoadFile(dlg.GetPath(), true);
    opencl_img->Refresh();
    
    layersinfo->Insert(dlg.GetFilename(), layersinfo->GetCount());
    layersinfo->Check(layersinfo->GetCount() - 1);
  }
}
void AccelerPaint::SaveRender(wxCommandEvent& event)
{
  //Return if an image isn't loaded yet
  if(opencl_img->LayerCount() == 0)
  {
    return;
  }
  //Simply call save dialog
  wxFileDialog  dlg( this, _T("Save As..."), wxEmptyString, wxEmptyString, _T("Acceler Source (*.acl)|*.acl|PNG files (*.png)|*.png|JPG files (*.jpg)|*.jpg|BMP files (*.bmp)|*.bmp"), wxFD_SAVE);

	if( dlg.ShowModal() == wxID_OK )
	{
    //Saving acceler files
    if(dlg.GetFilterIndex() == 0)
    {
      wxFileOutputStream output_stream(dlg.GetPath());
      if (!output_stream.IsOk())
      {
          wxLogError("Unable to aquire stream from acceler saving.", dlg.GetPath());
          return;
      }
      Acceler_Data datagram;
      datagram.i_height = opencl_img->GetCanvasHeight();
      datagram.i_width = opencl_img->GetCanvasWidth();
      datagram.layer_count = opencl_img->LayerCount();
      datagram.signature = ACCELER_SIGNITURE;
      datagram.version = ACCELER_VERSION;
      output_stream.WriteAll(&datagram, sizeof(datagram));
      for(unsigned layer = 0; layer < opencl_img->LayerCount(); layer++)
      {
        Layer_Data discriptor;
        discriptor.opacity = opencl_img->GetOpacity(layer);
        discriptor.visible = opencl_img->GetVisability(layer);
        output_stream.WriteAll(&discriptor, sizeof(discriptor));
        wxString name = layersinfo->GetString(layer);
        int layer_size = name.size();
        output_stream.WriteAll(&layer_size, sizeof(layer_size));
        output_stream.WriteAll(name.c_str(), layer_size);
        output_stream.WriteAll(opencl_img->GetRGBChannel(layer), datagram.i_height * datagram.i_width * COLOR_DEPTH);
        output_stream.WriteAll(opencl_img->GetAlphaChannel(layer), datagram.i_height * datagram.i_width);
      }
    }
    //Rendering an actual image
    else
    {
      wxImage render(*opencl_img->GetRender());

      render.SaveFile(dlg.GetPath());
    }
  }
}
void AccelerPaint::LayerChecked(wxCommandEvent& event)
{
  opencl_img->CheckVisability(event.GetInt(), layersinfo->IsChecked(event.GetInt()));
  opencl_img->Refresh();
}
void AccelerPaint::LayerChanged(wxCommandEvent& event)
{
  opacityctrl->SetValue(floor(opencl_img->GetOpacity(event.GetInt()) * 100 + 0.5));
}
void AccelerPaint::ImageBackground(wxPaintEvent& event)
{
  wxBufferedPaintDC dc(this);

  // draw a rectangle
  dc.SetBrush(wxBrush(BACKGROUND_COLOR));

  //Increase the rectangle size by 2 and recenter
  dc.DrawRectangle( -1, -1, this->GetSize().GetWidth() + 2, this->GetSize().GetHeight() + 2);
}
void AccelerPaint::ImageScroll(wxScrollEvent& event)
{
  opencl_img->Refresh();
}
 void AccelerPaint::ColorPicker(wxCommandEvent& event)
 {
   wxColourDialog  dlg(this);

	if( dlg.ShowModal() == wxID_OK )
	{
    pickedcolor = dlg.GetColourData().GetColour();
    ColorButton->SetBackgroundColour(pickedcolor);
  }
}
void AccelerPaint::ToolSelected(wxCommandEvent& event)
{
  for(unsigned i = 0; i < toolbuttons.size(); i++)
  {
    if(toolbuttons[i] == event.GetEventObject())
    {
      Toolsupdate(i);
      return;
    }
  }
}
void AccelerPaint::OpacityChanged(wxSpinEvent& event)
{
  if(!layersinfo->GetCount())
    return;

  float opacity = event.GetInt() / 100.0f;
  int index = layersinfo->GetSelection();
  if(index == wxNOT_FOUND)
    index = 0;

  opencl_img->SetOpacity(index, opacity);
  opencl_img->Refresh();
}
void AccelerPaint::BlurLayer(wxCommandEvent& event)
{
  if(!layersinfo->GetCount())
    return;

  int index = layersinfo->GetSelection();
  if(index == wxNOT_FOUND)
    index = 0;

  opencl_img->Blur(index);
  opencl_img->Refresh();
}
void AccelerPaint::Toolsupdate(int tool)
{
  for(unsigned i = 0; i < toolbuttons.size(); i++)
  {
    if(i == tool)
      toolbuttons[i]->Enable(false);
    else
      toolbuttons[i]->Enable(true);
  }
  selected_tool = tool;
}