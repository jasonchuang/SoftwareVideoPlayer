package com.jasonsoft.softwarevideoplayer.adapter;

import android.provider.MediaStore;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.provider.MediaStore.Video.VideoColumns;
import android.util.LruCache;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.CursorAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import com.jasonsoft.softwarevideoplayer.LoadThumbnailTask;
import com.jasonsoft.softwarevideoplayer.R;
import com.jasonsoft.softwarevideoplayer.cache.CacheManager;
import com.jasonsoft.softwarevideoplayer.data.AsyncDrawable;
import com.jasonsoft.softwarevideoplayer.data.LoadThumbnailParams;
import com.jasonsoft.softwarevideoplayer.data.LoadThumbnailResult;
import com.jasonsoft.softwarevideoplayer.data.MenuDrawerItem;
import com.jasonsoft.softwarevideoplayer.data.MenuDrawerCategory;

import java.util.List;

public class MenuAdapter extends CursorAdapter {

    public interface MenuListener {

        void onActiveViewChanged(View v);
    }

    class ViewHolder {
        ImageView thumbnailView;
        TextView title;
        TextView details;
    }

    private Context mContext;
    private final LayoutInflater mInflater;
    private Bitmap mDefaultPhotoBitmap;
    private LruCache<String, Bitmap> mMemoryCache;

    private List<Object> mItems;

    private MenuListener mListener;

    private int mActivePosition = -1;

    public MenuAdapter(Context context, Cursor c) {
        super(context, c);
        mContext = context;
        mInflater = LayoutInflater.from(context);
        mDefaultPhotoBitmap = BitmapFactory.decodeResource(context.getResources(), R.drawable.placeholder_empty);
    }

    public void setListener(MenuListener listener) {
        mListener = listener;
    }

    public void setActivePosition(int activePosition) {
        mActivePosition = activePosition;
    }

    @Override
    public View newView(Context context, Cursor cursor, ViewGroup parent) {
        final View view = mInflater.inflate(R.layout.video_list_item, null);
        return view;
    }

    @Override
    public void bindView(View view, Context context, Cursor cursor) {
        ViewHolder holder = (ViewHolder)view.getTag();
        if (null == holder) {
            holder =  populateViewHolder(view);
        }


        String details = cursor.getString(cursor.getColumnIndex(VideoColumns.DURATION)) + " - "
                + cursor.getString(cursor.getColumnIndex(VideoColumns.RESOLUTION));
        holder.title.setText(cursor.getString(cursor.getColumnIndex(VideoColumns.TITLE)));
        holder.details.setText(details);

//        long userId = cursor.getLong(cursor.getColumnIndex(VideoContact.COLUMN_CONTACT));
        long origId = cursor.getInt(cursor.getColumnIndex(VideoColumns._ID));
        loadThumbnail(origId, holder.thumbnailView);
    }
//    @Override
//    public View getView(int position, View convertView, ViewGroup parent) {
//        View v = convertView;
//        Object item = getItem(position);
//
//        if (item instanceof MenuDrawerCategory) {
//            if (v == null) {
//                v = LayoutInflater.from(mContext).inflate(R.layout.menu_row_category, parent, false);
//            }
//
//            ((TextView) v).setText(((MenuDrawerCategory) item).mTitle);
//
//        } else {
//            if (v == null) {
//                v = LayoutInflater.from(mContext).inflate(R.layout.menu_row_item, parent, false);
//            }
//
//            TextView tv = (TextView) v;
//            tv.setText(((MenuDrawerItem) item).mTitle);
//            tv.setCompoundDrawablesWithIntrinsicBounds(((MenuDrawerItem) item).mIconRes, 0, 0, 0);
//        }
//
//        v.setTag(R.id.mdActiveViewPosition, position);
//
//        if (position == mActivePosition) {
//            mListener.onActiveViewChanged(v);
//        }
//
//        return v;
//    }

    ViewHolder populateViewHolder(View view) {
        ViewHolder holder = new ViewHolder();
        holder.thumbnailView = (ImageView) view.findViewById(R.id.video_thumbnail);
        holder.title = (TextView) view.findViewById(R.id.video_title);
        holder.details = (TextView) view.findViewById(R.id.video_details);
        view.setTag(holder);
        return holder;
    }

    private void loadThumbnail(long origId, ImageView thumbnailView) {
        final String imageKey = String.valueOf(origId);
        final Bitmap bitmap = CacheManager.getInstance().getMemoryCache().get(imageKey);

        if (bitmap != null) {
        android.util.Log.d("jason", "Got imageKey:" +  imageKey + " from memory cache");
            thumbnailView.setImageBitmap(bitmap);
        } else if (cancelPotentialWork(origId, thumbnailView)) {
            final LoadThumbnailTask task = new LoadThumbnailTask(mContext, thumbnailView);
            final AsyncDrawable asyncDrawable = new AsyncDrawable(mDefaultPhotoBitmap, task);
            thumbnailView.setImageDrawable(asyncDrawable);
            task.execute(new LoadThumbnailParams(origId));
        }
    }

    /**
     * Returns true if the current work has been canceled or if there was no work in
     * progress on this image view.
     * Returns false if the work in progress deals with the same data. The work is not
     * stopped in that case.
     */
    public static boolean cancelPotentialWork(Object data, ImageView imageView) {
        final LoadThumbnailTask loadPhotoTask = LoadThumbnailTask.getLoadThumbnailTask(imageView);

        if (loadPhotoTask != null) {
            final Object bitmapData = loadPhotoTask.getData();
            if (bitmapData == null || !bitmapData.equals(data)) {
                loadPhotoTask.cancel(true);
            } else {
                // The same work is already in progress.
                return false;
            }
        }
        return true;
    }

}
