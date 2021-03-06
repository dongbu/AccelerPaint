#include "Accel_ImagePanel.h"
#include "wx\window.h"
#include "wx\dcbuffer.h"
#include <stack>

Accel_ImagePanel::Accel_ImagePanel(wxWindow* parent, wxScrollBar* hscroll, wxScrollBar* vscroll)
{
  ImagePanel = NULL;

  hscroll_ = hscroll;
  vscroll_ = vscroll;
  scroll_size = hscroll->GetSize().GetHeight();
  need_paint = FULL_REPAINT;

  Create(parent, 0, 0,  parent->GetSize().GetX() - scroll_size, parent->GetSize().GetY() - scroll_size);
}
void Accel_ImagePanel::Create(wxWindow* parent, int x, int y, int width, int height)
{
  parent_ = parent;
  id = wxNewId();
  draw_id = wxNewId();
  Background.LoadFile("./resources/Background.png");
  SetSize(width, height);
  ImagePanel = new wxPanel(parent, id, wxPoint(x, y), wxSize(width, height), wxFULL_REPAINT_ON_RESIZE|wxTAB_TRAVERSAL, _T("ID_ImagePanel"));
  ImagePanel->Connect(wxEVT_PAINT,(wxObjectEventFunction)&Accel_ImagePanel::Update, 0, this);
  ImagePanel->SetDoubleBuffered(true);
  ImagePanel->SetBackgroundStyle(wxBG_STYLE_PAINT);
}
void Accel_ImagePanel::Remove()
{
  ImagePanel->Destroy();
  Background.Destroy();
  for(unsigned layer = 0; layer < Layers.size(); layer++)
  {
    Layers[layer].Image->Destroy();
    delete Layers[layer].Image;
  }
}
void Accel_ImagePanel::Refresh()
{
  need_paint = FULL_REPAINT;
  ImagePanel->Refresh();
}
void Accel_ImagePanel::RenderImage()
{      
  image img_final;
  img_final.rgb_data = Render.GetData();
  img_final.alpha_data = Render.GetAlpha();
  img_final.pos_data.width = Render.GetSize().GetWidth();
  img_final.pos_data.height = Render.GetSize().GetHeight();
    
  rect fill_r;
  fill_r.width = Render.GetSize().GetWidth();
  fill_r.height = Render.GetSize().GetHeight();
  fill_r.x = fill_r.y = 0;

  device.Fill(img_final, fill_r, color());

  image next_img;
  next_img.pos_data = img_final.pos_data;
  for(int layer = LayerCount() - 1; layer >= 0; layer--)
  {
    if(GetVisability(layer))
    {
      next_img.rgb_data = GetRGBChannel(layer);
      next_img.alpha_data = GetAlphaChannel(layer);
      next_img.opacity = GetOpacity(layer);
      device.Blend(img_final, next_img, &Layers[layer].Channels);
    }

  }
}
void Accel_ImagePanel::Update(wxPaintEvent& event)
{
  if(ImagePanel)
  {
    int rend_width, rend_height;

    //Update scroll width
    if(img_width < parent_->GetSize().GetWidth() - scroll_size)
    {
      rend_width = img_width;
      hscroll_->Enable(false);
      x_off = 0;
    }
    else
    {
      rend_width = parent_->GetSize().GetWidth() - scroll_size;
      if(!hscroll_->IsEnabled())
      {
        hscroll_->SetThumbPosition(0);
        hscroll_->Enable(true);
      }
      if(hscroll_->GetRange() != img_width - rend_width)
        hscroll_->SetScrollbar(hscroll_->GetThumbPosition(), hscroll_->GetThumbSize(), (img_width - rend_width), 10);
      x_off = hscroll_->GetThumbPosition();
    }

    //Update scroll height
    if(img_height < parent_->GetSize().GetHeight() - scroll_size)
    {
      rend_height = img_height;
      vscroll_->Enable(false);
      y_off = 0;
    }
    else
    {
      rend_height = parent_->GetSize().GetHeight() - scroll_size;
      if(!vscroll_->IsEnabled())
      {
        vscroll_->SetThumbPosition(0);
        vscroll_->Enable(true);
      }
      if(vscroll_->GetRange() != img_height - rend_height)
        vscroll_->SetScrollbar(vscroll_->GetThumbPosition(), vscroll_->GetThumbSize(), img_height - rend_height, 10);
      y_off = vscroll_->GetThumbPosition();
    }

    //Make the image the right size and position
    ImagePanel->SetSize(rend_width, rend_height);
    ImagePanel->SetPosition(wxPoint((parent_->GetSize().GetWidth() - rend_width - scroll_size) / 2, (parent_->GetSize().GetHeight() - rend_height - scroll_size) / 2));
	  
    //Render the image
    wxBufferedPaintDC dc(ImagePanel);
    for(int y = 0; y < ImagePanel->GetSize().GetHeight(); y += 128)
      for(int x = 0; x < ImagePanel->GetSize().GetWidth(); x += 128)
        dc.DrawBitmap(Background,x,y, true);
    //for(unsigned layer = 0; layer < Layers.size(); layer++)
    //  if(Layers[layer].Enabled)
    //    dc.DrawBitmap(*Layers[layer].Image, -x_off, -y_off, true);
    if(need_paint == FULL_REPAINT)
    {
      need_paint = NO_REPAINT;
      RenderImage();
    }
    dc.DrawBitmap(Render, -x_off, -y_off, true);
  }
}
void Accel_ImagePanel::LoadFile(const wxString& name, bool new_layer)
{  
  //Update Fix:
  //quites iCCP warning for PNG files. Temporary fix that quites ALL warnings.
  wxLogNull Nolog;

  //Loading a file is going to require a repaint.
  need_paint = FULL_REPAINT;

  //Load the image into the layer it belongs.
  //If it's not a new layer then wipe the layers
  if(Layers.size() > 0 && !new_layer)
  {
    for(unsigned layer = 1; layer < Layers.size(); layer++)
    {
      Layers[layer].Image->Destroy();
      delete Layers[layer].Image;
    }
    Layers.resize(1);
  }
  else
  {
    Layer new_lay;
    new_lay.Image = new wxImage();
    new_lay.Enabled = true;
    new_lay.Opacity = 1.0f;
    Layers.insert(Layers.begin(), new_lay);
  }
  Layers[0].Enabled = true;
  Layers[0].Image->LoadFile(name);

  //Set alpha channels for images that don't have them.
  if(!Layers[0].Image->GetAlpha())
  {
    Layers[0].Image->InitAlpha();
  }
  
  //If it's not a new layer then update the canvas size and position.
  if(!new_layer)
  {
    wxImage* ImagePtr = Layers[0].Image;

    img_width = ImagePtr->GetWidth();
    img_height = ImagePtr->GetHeight();

    //Resize image panel using this as our base canvas now.
    ImagePanel->SetSize(img_width, img_height);
    ImagePanel->SetPosition(wxPoint((parent_->GetSize().GetWidth() - img_width - scroll_size) / 2, 
                            (parent_->GetSize().GetHeight() - img_height - scroll_size) / 2));

    //Reset the render target
    Render = ImagePtr->Copy();
  }
  //Resize to the canvas if it is a new layer
  else
  {
    if(Layers[0].Image->GetSize() != Layers[Layers.size() - 1].Image->GetSize())
    {
      Layers[0].Image->Resize(Layers[Layers.size() - 1].Image->GetSize(), wxPoint(0, 0));
    }
  }
}
void Accel_ImagePanel::LoadFile(int width, int height, unsigned char* data, unsigned char* alpha, channel channels, bool new_layer)
{  
  //Update Fix:
  //quites iCCP warning for PNG files. Temporary fix that quites ALL warnings.
  wxLogNull Nolog;

  //Loading a file is going to require a repaint.
  need_paint = FULL_REPAINT;

  //Load the image into the layer it belongs.
  //If it's not a new layer then wipe the layers
  if(Layers.size() > 0 && !new_layer)
  {
    for(unsigned layer = 1; layer < Layers.size(); layer++)
    {
      Layers[layer].Image->Destroy();
      delete Layers[layer].Image;
    }
    Layers.clear();
  }

  Layer new_lay;
  new_lay.Image = new wxImage(width, height, data, alpha);
  new_lay.Enabled = true;
  new_lay.Opacity = 1.0f;
  new_lay.Channels = channels;
  Layers.insert(Layers.begin(), new_lay);
  
  //If it's not a new layer then update the canvas size and position.
  if(!new_layer)
  {
    wxImage* ImagePtr = Layers[0].Image;

    img_width = ImagePtr->GetWidth();
    img_height = ImagePtr->GetHeight();
    
    //Resize image panel using this as our base canvas now.
    ImagePanel->SetSize(img_width, img_height);
    ImagePanel->SetPosition(wxPoint((parent_->GetSize().GetWidth() - img_width - scroll_size) / 2, 
                            (parent_->GetSize().GetHeight() - img_height - scroll_size) / 2));

    //Reset the render target
    Render = ImagePtr->Copy();
  }
}
void Accel_ImagePanel::Delete(unsigned layer)
{
  Layers[layer].Image->Destroy();
  delete Layers[layer].Image;
  Layers.erase(Layers.begin() + layer);

  Refresh();
}
void Accel_ImagePanel::Duplicate(unsigned layer)
{  
  //Update Fix:
  //quites iCCP warning for PNG files. Temporary fix that quites ALL warnings.
  wxLogNull Nolog;

  //Loading a file is going to require a repaint.
  need_paint = FULL_REPAINT;

  Layer new_lay;
  new_lay.Image = new wxImage(Layers[layer].Image->Copy());
  new_lay.Enabled = true;
  new_lay.Opacity = 1.0f;
  new_lay.Channels = Layers[layer].Channels;
  Layers.insert(Layers.begin(), new_lay);

}
void Accel_ImagePanel::Invert(unsigned layer)
{
  device.Simple(ImageData(layer), "Inverter");

  Refresh();
}
void Accel_ImagePanel::Threshold(unsigned layer)
{
  device.Simple(ImageData(layer), "Threshold");

  Refresh();
}
void Accel_ImagePanel::Greyscale(unsigned layer)
{
  device.Simple(ImageData(layer), "Greyscale");

  Refresh();
}
void Accel_ImagePanel::Blur(unsigned layer)
{
  device.Blur(ImageData(layer));

  Refresh();
}
void Accel_ImagePanel::Sobel(unsigned layer)
{
  device.Sobel(ImageData(layer));

  Refresh();
}
void Accel_ImagePanel::BucketFill(int layer, unsigned x, unsigned y, unsigned r, unsigned g, unsigned b)
{
  if(!Layers[layer].Enabled)
    return;
  x += x_off;
  y += y_off;
  wxImage* img = Layers[layer].Image;
  std::stack<Pixel> pixels;
  pixels.push(Pixel(x , y));

  bool* mem_map = new bool[img_height * img_width]();

  int red = img->GetRed(x, y);
  int blue = img->GetBlue(x, y);
  int green = img->GetGreen(x, y);
  int alpha = img->GetAlpha(x, y);
  
  while(!pixels.empty())
  {
    int px = pixels.top().x;
    int py = pixels.top().y;
    pixels.pop();
    int diffrence = abs(img->GetRed(px, py) - red);
    diffrence += abs(img->GetBlue(px, py) - blue);
    diffrence += abs(img->GetGreen(px, py) - green);
    diffrence += abs(img->GetAlpha(px, py) - alpha);
    if(diffrence > FILL_TOLERANCE || mem_map[py * img_width + px])
      continue;
    Layers[layer].Image->SetRGB(px, py, r, g, b);
    Layers[layer].Image->SetAlpha(px, py, 255);
    for(int i = -1; i <= 1; i++)
    {
      for(int j = -1; j <= 1; j++)
      {
        if(px + i < img_width && px + i >= 0 &&
           py + j < img_height && py + j >= 0 && !mem_map[(py + j) * img_width + (px + i)])
        {
          mem_map[py * img_width + px] = true;
          pixels.push(Pixel(px + i, py + j));
        }
      }
    }
  }
  delete[] mem_map;
  Refresh();
}
void Accel_ImagePanel::CheckVisability(int index, bool state)
{
  Layers[index].Enabled = state;
}
void Accel_ImagePanel::SetOpacity(int index, float opacity)
{
  Layers[index].Opacity = opacity;
}
unsigned char* Accel_ImagePanel::GetRGBChannel(unsigned layer)
{
  return Layers[layer].Image->GetData();
}
unsigned char* Accel_ImagePanel::GetAlphaChannel(unsigned layer)
{
  return Layers[layer].Image->GetAlpha();
}
bool Accel_ImagePanel::GetVisability(unsigned layer)
{
  return Layers[layer].Enabled;
}
float Accel_ImagePanel::GetOpacity(unsigned layer)
{
  return Layers[layer].Opacity;
}
unsigned Accel_ImagePanel::GetCanvasWidth()
{
  return img_width;
}
unsigned Accel_ImagePanel::GetCanvasHeight()
{
  return img_height;
} 
wxImage* Accel_ImagePanel::GetRender()
{
  return &Render;
}

unsigned Accel_ImagePanel::LayerCount()
{
  return Layers.size();
}
std::vector<Layer>* Accel_ImagePanel::GetLayers()
{
  return &Layers;
}
image Accel_ImagePanel::ImageData(unsigned layer)
{
  need_paint = FULL_REPAINT;

  image img;
  img.rgb_data = Layers[layer].Image->GetData();
  img.alpha_data = Layers[layer].Image->GetAlpha();
  img.pos_data.width = Layers[layer].Image->GetSize().GetWidth();
  img.pos_data.height = Layers[layer].Image->GetSize().GetHeight();

  return img;
}